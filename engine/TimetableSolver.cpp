#include "engine/TimetableSolver.h"
#include "engine/ScoreCalculator.h"
#include "core/ds/Stack.h"
#include "core/ds/MinHeap.h"
#include "core/ds/HashTable.h"
#include "core/ds/Graph.h"
#include "core/algo/Sorting.h"
#include <cstddef>
#include <string>

namespace {

struct Candidate {
    const Section* section = nullptr;
    int index = -1;
};

struct WorstOnTop {
    bool operator()(const Timetable& a, const Timetable& b) const {
        return a.score.total < b.score.total;
    }
};

struct Context {
    const Prefs* prefs = nullptr;
    std::atomic<bool>* cancel = nullptr;
    DynArray<DynArray<Candidate>> candidates;
    Graph conflicts{0};
    Stack<Candidate> chosen;
    MinHeap<Timetable, WorstOnTop> bestHeap;
    long long totalValid = 0;
    bool cancelled = false;
};

bool allowedSection(const Section& section, const Prefs& prefs) {
    for (std::size_t i = 0; i < section.meetings.size(); ++i) {
        const TimeSlot& meeting = section.meetings[i];
        if (meeting.day >= 0 && meeting.day < 7 && prefs.freeDay[meeting.day]) return false;
        if (prefs.hardNoEarly && meeting.startMin < prefs.earliestStartMin) return false;
    }
    return true;
}

void saveCompleteTimetable(Context& context) {
    Timetable timetable;
    const DynArray<Candidate>& current = context.chosen.items();

    for (std::size_t i = 0; i < current.size(); ++i) {
        timetable.chosen.push_back(current[i].section);
    }

    timetable.score = ScoreCalculator::compute(timetable, *context.prefs);
    if (timetable.score.maxGap > context.prefs->maxGapMin) return;

    ++context.totalValid;

    const std::size_t limit = context.prefs->topK > 0
        ? static_cast<std::size_t>(context.prefs->topK) : 1;

    if (context.bestHeap.size() < limit) {
        context.bestHeap.push(timetable);
    } else if (timetable.score.total > context.bestHeap.top().score.total) {
        context.bestHeap.pop();
        context.bestHeap.push(timetable);
    }
}

void backtrack(Context& context, std::size_t depth) {
    if (context.cancelled) return;
    if (context.cancel && context.cancel->load()) {
        context.cancelled = true;
        return;
    }

    if (depth == context.candidates.size()) {
        saveCompleteTimetable(context);
        return;
    }

    const DynArray<Candidate>& options = context.candidates[depth];
    for (std::size_t i = 0; i < options.size(); ++i) {
        const Candidate& candidate = options[i];
        bool clash = false;
        const DynArray<Candidate>& current = context.chosen.items();

        for (std::size_t j = 0; j < current.size(); ++j) {
            if (context.conflicts.hasEdge(candidate.index, current[j].index)) {
                clash = true;
                break;
            }
        }

        if (clash) continue;

        context.chosen.push(candidate);
        backtrack(context, depth + 1);
        context.chosen.pop();
        if (context.cancelled) return;
    }
}

} // namespace

SolveResult TimetableSolver::solve(const DynArray<Course>& courses,
                                   const Prefs& prefs,
                                   std::atomic<bool>* cancel) {
    SolveResult result;

    if (courses.empty()) {
        result.message = "Add at least one course first.";
        return result;
    }

    for (std::size_t i = 0; i < courses.size(); ++i) {
        if (courses[i].sections.empty()) {
            result.message = "Course " + courses[i].code + " has no sections.";
            return result;
        }
    }

    HashTable<int, int, IntHash> locked;
    for (std::size_t i = 0; i < prefs.lockedSectionIds.size(); ++i) {
        locked.insert(prefs.lockedSectionIds[i], 1);
    }

    DynArray<const Section*> allSections;
    DynArray<int> courseOfSection;
    DynArray<DynArray<Candidate>> perCourse;
    HashTable<int, int, IntHash> foundLocked;

    for (std::size_t courseIndex = 0; courseIndex < courses.size(); ++courseIndex) {
        DynArray<Candidate> options;
        DynArray<Candidate> lockedOptions;

        for (ListNode<Section>* node = courses[courseIndex].sections.head();
             node; node = node->next) {
            Candidate candidate;
            candidate.section = &node->val;
            candidate.index = static_cast<int>(allSections.size());

            allSections.push_back(&node->val);
            courseOfSection.push_back(static_cast<int>(courseIndex));

            if (locked.contains(node->val.id)) {
                lockedOptions.push_back(candidate);
                foundLocked.insert(node->val.id, 1);
            }
            if (allowedSection(node->val, prefs)) options.push_back(candidate);
        }

        if (lockedOptions.size() > 1) {
            result.message = "More than one section is locked for course " + courses[courseIndex].code + ".";
            return result;
        }
        if (!lockedOptions.empty()) options = lockedOptions;

        if (options.empty()) {
            result.message = "No section of " + courses[courseIndex].code + " passes your filters.";
            return result;
        }

        perCourse.push_back(options);
    }

    for (std::size_t i = 0; i < prefs.lockedSectionIds.size(); ++i) {
        if (!foundLocked.contains(prefs.lockedSectionIds[i])) {
            result.message = "A locked section ID was not found: " + std::to_string(prefs.lockedSectionIds[i]);
            return result;
        }
    }

    Context context;
    context.prefs = &prefs;
    context.cancel = cancel;
    context.conflicts = Graph(static_cast<int>(allSections.size()));

    for (std::size_t i = 0; i < allSections.size(); ++i) {
        for (std::size_t j = i + 1; j < allSections.size(); ++j) {
            if (courseOfSection[i] != courseOfSection[j]
                && allSections[i]->clashesWith(*allSections[j])) {
                context.conflicts.addEdge(static_cast<int>(i), static_cast<int>(j));
            }
        }
    }

    DynArray<int> order;
    for (std::size_t i = 0; i < perCourse.size(); ++i) {
        order.push_back(static_cast<int>(i));
    }

    insertionSort(order, [&perCourse](int a, int b) {
        return perCourse[static_cast<std::size_t>(a)].size()
             < perCourse[static_cast<std::size_t>(b)].size();
    });

    for (std::size_t i = 0; i < order.size(); ++i) {
        context.candidates.push_back(perCourse[static_cast<std::size_t>(order[i])]);
    }

    backtrack(context, 0);

    result.totalValid = context.totalValid;
    result.cancelled = context.cancelled;
    DynArray<Timetable> found = context.bestHeap.drain();
    mergeSort(found, [](const Timetable& a, const Timetable& b) {
        return a.score.total > b.score.total;
    });
    result.best = found;

    if (context.cancelled) {
        result.message = "Cancelled. Showing what was found so far.";
    } else if (context.totalValid == 0) {
        bool identifiedPair = false;
        for (std::size_t a = 0; a < perCourse.size() && !identifiedPair; ++a) {
            for (std::size_t b = a + 1; b < perCourse.size() && !identifiedPair; ++b) {
                bool everyPairClashes = true;
                for (std::size_t x = 0; x < perCourse[a].size(); ++x) {
                    for (std::size_t y = 0; y < perCourse[b].size(); ++y) {
                        if (!context.conflicts.hasEdge(perCourse[a][x].index,
                                                       perCourse[b][y].index)) {
                            everyPairClashes = false;
                        }
                    }
                }
                if (everyPairClashes) {
                    result.conflictingCourses.push_back(courses[a].code);
                    result.conflictingCourses.push_back(courses[b].code);
                    result.message = "No valid timetable: every section of " + courses[a].code
                        + " clashes with every section of " + courses[b].code + ".";
                    identifiedPair = true;
                }
            }
        }
        if (!identifiedPair) {
            result.message = "No clash-free timetable exists for these courses and filters.";
        }
    } else {
        result.message = std::to_string(context.totalValid) + " valid timetables found, showing the best "
            + std::to_string(result.best.size()) + ".";
    }

    return result;
}
