#include "engine/StudyPlanner.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <string>
#include <utility>

namespace {

struct Win { int s, e; bool campus; };
struct Cell { int s, e; bool campus; };
struct Slot { int day, s, e; bool campus; };

void subtract(std::vector<Win>& wins, int s, int e) {
    std::vector<Win> out;
    for (const Win& w : wins) {
        if (e <= w.s || s >= w.e) { out.push_back(w); continue; }
        if (s > w.s) out.push_back({w.s, s, w.campus});
        if (e < w.e) out.push_back({e, w.e, w.campus});
    }
    wins.swap(out);
}

// Cuts free windows into focus blocks separated by breaks, up to `budget` minutes.
std::vector<Cell> cut(const std::vector<Win>& wins, const StudyPrefs& p, int budget) {
    std::vector<Cell> out;
    int used = 0;
    for (const Win& w : wins) {
        int t = w.s, run = 0;
        while (used < budget) {
            int len = std::min(p.focusMin, w.e - t);
            len = std::min(len, budget - used);
            if (len < p.minBlockMin) break;
            out.push_back({t, t + len, w.campus});
            used += len;
            t += len;
            ++run;
            t += (run % 2 == 0) ? p.longBreakMin : p.shortBreakMin;
            if (t >= w.e) break;
        }
        if (used >= budget) break;
    }
    return out;
}

int sumCells(const std::vector<Cell>& v) {
    int t = 0;
    for (const Cell& c : v) t += c.e - c.s;
    return t;
}

} // namespace

StudyPlan StudyPlanner::plan(const std::vector<StudyCourse>& courses,
                             const std::vector<StudyBusy>& weekly,
                             const StudyPrefs& pIn,
                             long long todayJd,
                             int nowMin) {
    StudyPlan out;
    out.startJd = todayJd;
    out.neededMin.assign(courses.size(), 0);
    out.plannedMin.assign(courses.size(), 0);

    StudyPrefs p = pIn;
    if (p.focusMin < 20) p.focusMin = 20;
    if (p.minBlockMin > p.focusMin) p.minBlockMin = p.focusMin;
    if (p.minBlockMin < 15) p.minBlockMin = 15;
    if (p.examBufferBeforeMin < 0) p.examBufferBeforeMin = 0;
    if (p.breakAfterExamMin < 0) p.breakAfterExamMin = 0;

    if (courses.empty()) { out.message = "Add at least one course with an exam date."; return out; }
    if (p.latestEndMin <= p.freeDayStartMin) {
        out.message = "Study must end after it starts. Check the start and end times.";
        return out;
    }

    // exam times per course (sanitised)
    std::vector<int> exS(courses.size()), exE(courses.size());
    for (std::size_t i = 0; i < courses.size(); ++i) {
        exS[i] = std::max(0, courses[i].examStartMin);
        exE[i] = courses[i].examEndMin > exS[i] ? courses[i].examEndMin : exS[i] + 120;
    }

    // ---- which courses still have an exam ahead --------------------------
    std::vector<int> examIdx(courses.size(), -1);
    int valid = 0;
    int lastIdx = 0;
    for (std::size_t i = 0; i < courses.size(); ++i) {
        const long long d = courses[i].examJd - todayJd;
        if (d <= 0) {
            out.warnings.push_back(courses[i].code + ": the exam is today or already passed, so it was left out.");
            continue;
        }
        if (d > 180) {
            out.warnings.push_back(courses[i].code + ": the exam is more than 180 days away, so it was left out.");
            continue;
        }
        examIdx[i] = static_cast<int>(d);
        ++valid;
        lastIdx = std::max(lastIdx, static_cast<int>(d));
    }
    if (valid == 0) { out.message = "No course has an exam date in the future."; return out; }

    const int N = lastIdx + 1;
    out.days.resize(static_cast<std::size_t>(N));

    // ---- build every day: fixed items and free windows -----------------------
    std::vector<std::vector<Win>> windows(static_cast<std::size_t>(N));
    std::vector<int> capacity(static_cast<std::size_t>(N), 0);

    auto dayMaxFor = [&](const StudyDay& day) -> int {
        if (!day.exams.empty()) return p.maxExamDayStudyMin;
        return day.uniEndMin >= 0 ? p.maxWeekdayStudyMin : p.maxFreeDayStudyMin;
    };

    for (int d = 0; d < N; ++d) {
        StudyDay& day = out.days[static_cast<std::size_t>(d)];
        day.jd = todayJd + d;
        day.weekday = static_cast<int>(((day.jd % 7) + 7) % 7);

        // exams of this day first: they remove classes from the day
        for (std::size_t i = 0; i < courses.size(); ++i)
            if (examIdx[i] == d) {
                day.examCourses.push_back(static_cast<int>(i));
                day.exams.push_back({static_cast<int>(i), exS[i], exE[i]});
            }
        std::sort(day.exams.begin(), day.exams.end(),
                  [](const StudyExam& a, const StudyExam& b) { return a.startMin < b.startMin; });
        const bool examDay = !day.exams.empty();

        std::vector<std::pair<int, int>> uni;
        std::vector<std::pair<int, int>> ext;
        for (const StudyBusy& b : weekly) {
            if (b.endMin <= b.startMin) continue;
            // Dated items (events) apply only on their exact day; items without a
            // date (classes) repeat every week on their weekday.
            if (b.jd >= 0) { if (b.jd != day.jd) continue; }
            else if (b.weekday != day.weekday) continue;
            if (examDay && b.kind != BusyKind::ExternalEvent) continue;  // no classes on exam day
            day.busy.push_back(b);
            if (b.kind == BusyKind::ExternalEvent) ext.push_back({b.startMin, b.endMin});
            else {
                uni.push_back({b.startMin, b.endMin});
                day.uniEndMin = std::max(day.uniEndMin, b.endMin);
            }
        }
        std::sort(day.busy.begin(), day.busy.end(),
                  [](const StudyBusy& a, const StudyBusy& b) { return a.startMin < b.startMin; });
        std::sort(uni.begin(), uni.end());

        bool examSoon = false;
        for (std::size_t i = 0; i < courses.size(); ++i)
            if (examIdx[i] > d && examIdx[i] - d <= 2) examSoon = true;
        day.restDay = (p.restWeekday == day.weekday) && !examSoon && !examDay;

        if (day.restDay) continue;

        std::vector<Win>& wins = windows[static_cast<std::size_t>(d)];
        if (examDay) {
            // The whole day is free of classes; the exams themselves are blocked
            // out together with a buffer before and a rest after.
            wins.push_back({p.freeDayStartMin, p.latestEndMin, false});
            for (const StudyExam& ex : day.exams)
                subtract(wins, ex.startMin - p.examBufferBeforeMin, ex.endMin + p.breakAfterExamMin);
        } else if (day.uniEndMin < 0) {
            wins.push_back({p.freeDayStartMin, p.latestEndMin, false});
        } else {
            if (p.useCampusGaps) {
                int cursor = p.freeDayStartMin;
                for (const auto& iv : uni) {
                    if (iv.first > cursor) {
                        const int gs = cursor + (cursor == p.freeDayStartMin ? 0 : 10);
                        const int ge = iv.first - 10;
                        if (ge - gs >= p.minCampusGapMin) wins.push_back({gs, ge, true});
                    }
                    cursor = std::max(cursor, iv.second);
                }
            }
            wins.push_back({day.uniEndMin + p.breakAfterUniMin, p.latestEndMin, false});
        }

        if (d == 0) subtract(wins, 0, ((nowMin + 15 + 4) / 5) * 5);
        for (const auto& iv : ext)
            subtract(wins, iv.first - p.externalMarginMin, iv.second + p.externalMarginMin);
        subtract(wins, p.dinnerStartMin, p.dinnerStartMin + p.dinnerLenMin);
        subtract(wins, p.lunchStartMin, p.lunchStartMin + p.lunchLenMin);

        std::vector<Win> kept;
        for (const Win& w : wins)
            if (w.e - w.s >= p.minBlockMin) kept.push_back(w);
        wins.swap(kept);

        capacity[static_cast<std::size_t>(d)] = sumCells(cut(wins, p, dayMaxFor(day)));
    }

    // ---- how much each course needs ------------------------------------
    std::vector<int> need(courses.size(), 0);
    double totalNeed = 0;
    for (std::size_t i = 0; i < courses.size(); ++i) {
        if (examIdx[i] < 0) continue;
        const int diff = std::max(1, std::min(10, courses[i].difficulty));
        need[i] = diff * 100;
        totalNeed += need[i];
    }
    long long totalCap = 0;
    for (int c : capacity) totalCap += c;
    if (totalCap <= 0) {
        out.message = "There is no free time to study before the exams. Relax the study hours or the rest day.";
        return out;
    }
    if (totalNeed * 1.1 > static_cast<double>(totalCap)) {
        const double scale = static_cast<double>(totalCap) * 0.95 / totalNeed;
        for (int& n : need) n = static_cast<int>(n * scale);
        totalNeed *= scale;
        out.warnings.push_back("There is not enough free time to study every course fully. The plan covers about "
                               + std::to_string(static_cast<int>(scale * 100)) + "% of the ideal time.");
    }
    for (std::size_t i = 0; i < courses.size(); ++i) out.neededMin[i] = need[i];

    // ---- daily budgets: spread the load, heavier close to exams ----------
    std::vector<Slot> slots;
    for (int d = 0; d < N; ++d) {
        const int cap = capacity[static_cast<std::size_t>(d)];
        if (cap <= 0) continue;
        StudyDay& day = out.days[static_cast<std::size_t>(d)];
        double budget = std::ceil(totalNeed * 1.1 * cap / static_cast<double>(totalCap));
        bool ramp = !day.exams.empty();
        for (std::size_t i = 0; i < courses.size(); ++i)
            if (examIdx[i] > d && examIdx[i] - d <= 3) ramp = true;
        if (ramp) budget *= 1.35;
        const int dayMax = dayMaxFor(day);
        int b = static_cast<int>(std::min<double>(budget, std::min(cap, dayMax)));
        if (b < p.minBlockMin) continue;
        for (const Cell& c : cut(windows[static_cast<std::size_t>(d)], p, b))
            slots.push_back({d, c.s, c.e, c.campus});
    }

    // A course can be studied in a slot if its exam is on a later day, or on the
    // same day and the slot ends before the exam starts.
    auto usable = [&](std::size_t c, const Slot& s) {
        if (examIdx[c] < 0) return false;
        if (examIdx[c] > s.day) return true;
        return examIdx[c] == s.day && s.e <= exS[c];
    };

    // ---- assign courses to slots ---------------------------------------
    const std::size_t n = slots.size();
    std::vector<std::vector<int>> availAfter(courses.size(), std::vector<int>(n + 1, 0));
    for (std::size_t c = 0; c < courses.size(); ++c) {
        if (examIdx[c] < 0) continue;
        for (std::size_t i = n; i-- > 0;)
            availAfter[c][i] = availAfter[c][i + 1] + (usable(c, slots[i]) ? slots[i].e - slots[i].s : 0);
    }

    std::vector<int> remaining = need;
    int prev = -1, run = 0, prevDay = -1;
    for (std::size_t i = 0; i < n; ++i) {
        const Slot& s = slots[i];
        const int len = s.e - s.s;
        if (s.day != prevDay) { prev = -1; run = 0; prevDay = s.day; }

        int best = -1;
        double bestScore = -1e9;
        for (std::size_t c = 0; c < courses.size(); ++c) {
            if (!usable(c, s) || remaining[c] <= 0) continue;
            const int diff = std::max(1, std::min(10, courses[c].difficulty));
            double score = static_cast<double>(remaining[c]) / std::max(1, availAfter[c][i]);
            if (examIdx[c] == s.day) score += 0.6;                 // last chance, exam later today
            else if (examIdx[c] - s.day == 1) score += 0.3;
            score += 0.02 * diff;
            if (s.s < 16 * 60) score += 0.015 * (diff - 5);        // hard subjects while fresh
            else if (s.s >= 20 * 60) score -= 0.015 * (diff - 5);  // lighter ones late
            if (static_cast<int>(c) == prev) score -= (run >= 2) ? 0.5 : 0.05;
            if (score > bestScore) { bestScore = score; best = static_cast<int>(c); }
        }

        BlockKind kind = BlockKind::Study;
        if (best < 0) {
            // every target is met: use the slot to revise the nearest exam
            long long nearest = LLONG_MAX;
            for (std::size_t c = 0; c < courses.size(); ++c) {
                if (!usable(c, s)) continue;
                const long long key = static_cast<long long>(examIdx[c]) * 1440 + exS[c];
                if (key < nearest) { nearest = key; best = static_cast<int>(c); }
            }
            if (best < 0) continue;
            kind = BlockKind::Revision;
        } else {
            remaining[static_cast<std::size_t>(best)] -= len;
            if (examIdx[static_cast<std::size_t>(best)] == s.day) kind = BlockKind::ExamDay;
            else if (examIdx[static_cast<std::size_t>(best)] - s.day == 1) kind = BlockKind::ExamEve;
        }

        if (best == prev) ++run; else { prev = best; run = 1; }
        StudyDay& day = out.days[static_cast<std::size_t>(s.day)];
        day.blocks.push_back({best, s.s, s.e, kind, s.campus});
        out.plannedMin[static_cast<std::size_t>(best)] += len;
    }

    int totalPlanned = 0;
    for (StudyDay& day : out.days) {
        if (!day.blocks.empty()) day.studyFromMin = day.blocks.front().startMin;
        totalPlanned += day.studyMinutes();
    }
    for (std::size_t i = 0; i < courses.size(); ++i)
        if (examIdx[i] > 0 && out.plannedMin[i] < static_cast<int>(out.neededMin[i] * 0.85))
            out.warnings.push_back(courses[i].code + ": only " + std::to_string(out.plannedMin[i] / 60) + " h "
                                   + std::to_string(out.plannedMin[i] % 60) + " min fits before the exam.");

    out.ok = true;
    out.message = "Study plan ready: " + std::to_string(totalPlanned / 60) + " h " + std::to_string(totalPlanned % 60)
                  + " min over " + std::to_string(N) + " days for " + std::to_string(valid)
                  + (valid == 1 ? " exam." : " exams.");
    return out;
}