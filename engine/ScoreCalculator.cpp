#include "engine/ScoreCalculator.h"
#include "core/algo/Sorting.h"

ScoreBreakdown ScoreCalculator::compute(const Timetable& timetable, const Prefs& prefs) {
    ScoreBreakdown score;
    DynArray<TimeSlot> perDay[7];

    for (std::size_t i = 0; i < timetable.chosen.size(); ++i) {
        const Section* section = timetable.chosen[i];
        if (!section) continue;

        for (std::size_t j = 0; j < section->meetings.size(); ++j) {
            const TimeSlot& meeting = section->meetings[j];
            if (meeting.day < 0 || meeting.day > 6) continue;

            perDay[meeting.day].push_back(meeting);
            if (meeting.startMin < prefs.earliestStartMin) ++score.earlyClasses;
        }
    }

    for (int day = 0; day < 7; ++day) {
        DynArray<TimeSlot>& meetings = perDay[day];
        if (meetings.empty()) continue;

        ++score.daysOnCampus;
        insertionSort(meetings, [](const TimeSlot& a, const TimeSlot& b) {
            return a.startMin < b.startMin;
        });

        int latestEnd = meetings[0].endMin;
        for (std::size_t i = 1; i < meetings.size(); ++i) {
            int gap = meetings[i].startMin - latestEnd;
            if (gap > 0) {
                score.gapMinutes += gap;
                if (gap > score.maxGap) score.maxGap = gap;
            }
            if (meetings[i].endMin > latestEnd) latestEnd = meetings[i].endMin;
        }
    }

    for (int day = 0; day < 5; ++day) {
        if (perDay[day].empty()) ++score.freeDays;
    }

    score.gapPenalty = prefs.wGap * score.gapMinutes;
    score.freeDayBonus = prefs.wFreeDay * score.freeDays;
    score.earlyPenalty = prefs.wEarly * score.earlyClasses;
    score.daysPenalty = prefs.wDays * score.daysOnCampus;
    score.total = -score.gapPenalty + score.freeDayBonus
                  - score.earlyPenalty - score.daysPenalty;

    return score;
}
