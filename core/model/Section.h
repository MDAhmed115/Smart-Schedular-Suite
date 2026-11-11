#pragma once
#include "core/model/Common.h"
#include "core/model/TimeSlot.h"

struct Section {
    int id = -1;
    std::string courseCode;
    std::string label;
    DynArray<TimeSlot> meetings;

    bool clashesWith(const Section& other) const {
        for (std::size_t i = 0; i < meetings.size(); ++i) {
            for (std::size_t j = 0; j < other.meetings.size(); ++j) {
                if (meetings[i].clashes(other.meetings[j])) return true;
            }
        }
        return false;
    }
};
