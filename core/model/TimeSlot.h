#pragma once

struct TimeSlot {
    int day = 0;
    int startMin = 0;
    int endMin = 0;

    bool clashes(const TimeSlot& other) const {
        return day == other.day && startMin < other.endMin && other.startMin < endMin;
    }
};
