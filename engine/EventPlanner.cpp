#include "engine/EventPlanner.h"

#include "core/ds/Graph.h"
#include "core/ds/Queue.h"
#include "core/ds/HashTable.h"
#include "core/algo/Sorting.h"

#include <algorithm>
#include <climits>
#include <string>

// Converts minutes from the start of the event into a readable HhMM string.
// Examples: 150 -> "2h30", 65 -> "1h05", 0 -> "0h00".
std::string EventPlanner::formatTime(int minutes) {
    if (minutes < 0) {
        return "-" + formatTime(-minutes);
    }

    const int hours = minutes / 60;
    const int mins = minutes % 60;

    return std::to_string(hours) + "h" +
           (mins < 10 ? "0" : "") + std::to_string(mins);
}

EventPlan EventPlanner::plan(const DynArray<Task>& tasks) {
    EventPlan out;
    const int n = static_cast<int>(tasks.size());

    if (n == 0) {
        out.error = "Add at least one task.";
        return out;
    }

    // 1. Validate tasks and build a mapping: user task ID -> array position.
    // IDs are arbitrary; they are not assumed to be 0, 1, 2, ...
    HashTable<int, int, IntHash> index;

    for (int i = 0; i < n; ++i) {
        const Task& task = tasks[i];

        if (task.name.empty()) {
            out.error = "Task at position " + std::to_string(i) +
                        " has an empty name.";
            return out;
        }

        if (task.durationMin <= 0) {
            out.error = "Task " + task.name +
                        ": duration must be positive.";
            return out;
        }

        // HashTable::insert returns false when the key already exists.
        if (!index.insert(task.id, i)) {
            out.error = "Duplicate task id " + std::to_string(task.id) + ".";
            return out;
        }
    }

    // 2. Build directed dependency graph.
    // Edge dependency -> dependent task.
    Graph graph(n);

    for (int i = 0; i < n; ++i) {
        const Task& task = tasks[i];

        // Reject repeated dependency IDs. Without this validation, input data
        // can be confusing even if Graph suppresses duplicate edges.
        HashTable<int, bool, IntHash> seenDeps;

        for (std::size_t k = 0; k < task.deps.size(); ++k) {
            const int dependencyId = task.deps[k];
            int* dependencyPosition = index.find(dependencyId);

            if (dependencyPosition == nullptr) {
                out.error = "Task " + task.name +
                            " depends on unknown task id " +
                            std::to_string(dependencyId) + ".";
                return out;
            }

            if (*dependencyPosition == i) {
                out.error = "Task " + task.name +
                            " cannot depend on itself.";
                return out;
            }

            if (!seenDeps.insert(dependencyId, true)) {
                out.error = "Task " + task.name +
                            " lists dependency id " +
                            std::to_string(dependencyId) +
                            " more than once.";
                return out;
            }

            graph.addEdge(*dependencyPosition, i, true);
        }
    }

    // 3. Kahn's topological sort using the custom Queue.
    // waiting[i] is the number of dependencies that have not been processed.
    DynArray<int> waiting;
    for (int i = 0; i < n; ++i) {
        waiting.push_back(graph.inDegree(i));
    }

    Queue<int> ready;
    for (int i = 0; i < n; ++i) {
        if (waiting[i] == 0) {
            ready.enqueue(i);
        }
    }

    while (!ready.empty()) {
        const int current = ready.dequeue();
        out.order.push_back(current);

        const DynArray<int>& dependents = graph.neighbors(current);

        for (std::size_t k = 0; k < dependents.size(); ++k) {
            const int next = dependents[k];

            --waiting[next];
            if (waiting[next] == 0) {
                ready.enqueue(next);
            }
        }
    }

    // If not every task was emitted, the dependency graph contains a cycle.
    if (static_cast<int>(out.order.size()) != n) {
        DynArray<char> processed;
        for (int i = 0; i < n; ++i) {
            processed.push_back(0);
        }

        for (std::size_t k = 0; k < out.order.size(); ++k) {
            processed[out.order[k]] = 1;
        }

        std::string stuckNames;
        for (int i = 0; i < n; ++i) {
            if (!processed[i]) {
                if (!stuckNames.empty()) {
                    stuckNames += ", ";
                }
                stuckNames += tasks[i].name;
            }
        }

        out.cycleMsg = "Dependency cycle detected. Tasks blocked in the cycle "
                       "or downstream of it: " + stuckNames + ".";
        out.order.clear();
        return out;
    }

    // 4. Compute earliest start/end times in input-array order.
    // Earliest start = maximum finish time among all dependencies.
    for (int i = 0; i < n; ++i) {
        out.startMin.push_back(0);
        out.endMin.push_back(0);
    }

    for (std::size_t k = 0; k < out.order.size(); ++k) {
        const int taskPosition = out.order[k];
        const Task& task = tasks[taskPosition];

        int earliestStart = 0;

        for (std::size_t d = 0; d < task.deps.size(); ++d) {
            const int dependencyPosition = *index.find(task.deps[d]);

            if (out.endMin[dependencyPosition] > earliestStart) {
                earliestStart = out.endMin[dependencyPosition];
            }
        }

        // Protect against signed-int overflow in schedule arithmetic.
        if (task.durationMin > INT_MAX - earliestStart) {
            out.error = "Schedule time overflow while calculating task " +
                        task.name + ".";
            out.order.clear();
            out.startMin.clear();
            out.endMin.clear();
            return out;
        }

        out.startMin[taskPosition] = earliestStart;
        out.endMin[taskPosition] = earliestStart + task.durationMin;
    }

    // 5. Resource-overlap detection.
    // Sort task positions by start time, then compare intervals that can
    // overlap. A conflict is only reported if both tasks use the same
    // non-empty resource. Touching intervals are not overlaps.
    DynArray<int> byStart;
    for (int i = 0; i < n; ++i) {
        byStart.push_back(i);
    }

    mergeSort(byStart, [&](int a, int b) {
        if (out.startMin[a] != out.startMin[b]) {
            return out.startMin[a] < out.startMin[b];
        }
        // Deterministic tie-breaker for tasks starting at the same minute.
        return out.endMin[a] < out.endMin[b];
    });

    for (std::size_t a = 0; a < byStart.size(); ++a) {
        const int first = byStart[a];

        for (std::size_t b = a + 1; b < byStart.size(); ++b) {
            const int second = byStart[b];

            // Since starts are sorted, later tasks cannot overlap first
            // once their start is at or after first's end.
            if (out.startMin[second] >= out.endMin[first]) {
                break;
            }

            const std::string& resourceA = tasks[first].resource;
            const std::string& resourceB = tasks[second].resource;

            if (!resourceA.empty() && resourceA == resourceB) {
                out.overlaps.push_back(
                    tasks[first].name + " and " + tasks[second].name +
                    " both need " + resourceA + " from " +
                    formatTime(out.startMin[second]) + " to " +
                    formatTime(std::min(out.endMin[first], out.endMin[second])) +
                    ". Suggestion: start " + tasks[second].name + " at " +
                    formatTime(out.endMin[first]) + "."
                );
            }
        }
    }

    return out;
}
