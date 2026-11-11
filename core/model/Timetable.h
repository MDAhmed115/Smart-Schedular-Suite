#pragma once
#include "core/model/Section.h"

struct ScoreBreakdown {
    int gapMinutes = 0;
    int freeDays = 0;
    int earlyClasses = 0;
    int daysOnCampus = 0;
    int maxGap = 0;
    int gapPenalty = 0;
    int freeDayBonus = 0;
    int earlyPenalty = 0;
    int daysPenalty = 0;
    int total = 0;
};

struct Timetable {
    DynArray<const Section*> chosen;
    ScoreBreakdown score;
};

struct SolveResult {
    DynArray<Timetable> best;
    long long totalValid = 0;
    bool cancelled = false;
    std::string message;
    DynArray<std::string> conflictingCourses;
};
