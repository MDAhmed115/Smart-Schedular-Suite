#pragma once
#include <atomic>
#include "core/model/Course.h"
#include "core/model/Prefs.h"
#include "core/model/Timetable.h"

class TimetableSolver {
public:
    static SolveResult solve(const DynArray<Course>& courses,
                             const Prefs& prefs,
                             std::atomic<bool>* cancel = nullptr);
};
