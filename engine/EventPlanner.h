#pragma once

#include "core/ds/DynArray.h"

#include <string>

// One task in the event. Dependency values refer to Task::id, not array indexes.
struct Task {
    int id = 0;
    std::string name;
    std::string resource;
    int durationMin = 60;
    DynArray<int> deps;
};

// Result of planning. order contains input-array positions into the original tasks.
struct EventPlan {
    DynArray<int> order;
    DynArray<int> startMin;
    DynArray<int> endMin;
    DynArray<std::string> overlaps;
    std::string cycleMsg;
    std::string error;
};

class EventPlanner {
public:
    // Validate dependencies, topologically order tasks, compute earliest times,
    // and report overlapping use of the same resource.
    static EventPlan plan(const DynArray<Task>& tasks);

    // Convert minutes from event start to HhMM format (e.g. 65 -> "1h05").
    static std::string formatTime(int minutes);
};
