#pragma once
#include "core/model/Common.h"

struct Prefs {
    int earliestStartMin = 540;
    bool hardNoEarly = false;
    bool freeDay[7] = {false, false, false, false, false, false, false};
    int maxGapMin = 1440;
    DynArray<int> lockedSectionIds;
    int topK = 50;
    int wGap = 1;
    int wFreeDay = 120;
    int wEarly = 30;
    int wDays = 20;
};
