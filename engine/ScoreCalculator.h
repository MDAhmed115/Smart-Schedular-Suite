#pragma once
#include "core/model/Timetable.h"
#include "core/model/Prefs.h"

class ScoreCalculator {
public:
    static ScoreBreakdown compute(const Timetable& timetable, const Prefs& prefs);
};
