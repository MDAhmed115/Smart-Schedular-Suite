
#pragma once
#include <string>
#include <vector>

// Slot-based automatic weekly timetable.
//
// The teaching day is cut into equal "slots" (periods). A class occupies one or
// more consecutive slots; a multi-slot class also includes the short break that
// sits between its slots. The scheduler picks days and slots and, when random
// gaps are on, gives every day its own shape: some days are packed, some have
// one class early and one late, some finish early and some run to the end.
//
// The same engine also places one-off events. Free time is used first; an event
// that fits nowhere is put where it disturbs the least and the classes it
// overlaps are reported.

struct AutoCourse {
    int id = 0;
    std::string code;
    std::string name;
    int sessionsPerWeek = 2;   // how many times per week the class meets
    int slotsPerSession = 2;   // how many consecutive slots one meeting uses
};

struct AutoSession {
    int courseIndex = 0;   // index into the input vector
    int day = 0;           // 0 = Monday ... 6 = Sunday
    int startMin = 0;      // minutes since midnight
    int endMin = 0;
    int startSlot = 0;     // first slot index within the day
    int slotCount = 1;
};

struct AutoPrefs {
    int dayStartMin = 8 * 60;
    int dayEndMin = 16 * 60;
    bool dayAllowed[7] = {true, true, true, true, true, false, false};

    int slotMin = 50;          // length of ONE slot in minutes
    int slotBreakMin = 10;     // break between two consecutive slots
    int eventStartStepMin = 5; // event start-time granularity; external grids can align to their own slots

    bool longBreaks = true;    // random free blocks between classes
    int longBreakMinMin = 50;  // shortest free block to aim for (minutes)
    int longBreakMaxMin = 110; // longest free block to aim for (minutes)

    unsigned seed = 0;         // 0 = non-deterministic
};

struct AutoResult {
    bool ok = false;
    std::string message;
    std::vector<AutoSession> sessions;   // sorted by day, then start time
    int slotsPerDay = 0;
    bool longBreaksApplied = false;
};

struct AutoEvent {
    std::string name;
    int minutes = 60;
    int preferredDay = -1;   // -1 = any day, 0..6 = Monday..Sunday
    bool customTime = false; // true = keep the exact day and times below
    int customStartMin = 0;  // minutes since midnight
    int customEndMin = 0;    // minutes since midnight

    // Repetition (handled by the GUI when it builds the weekly view):
    //   0 = does not repeat (lasts only the week it was added in)
    //   1 = daily, 2 = weekly, 3 = monthly
    int repeat = 0;
    long long anchorJd = 0;  // Julian day of the first occurrence
};

// One thing an event overlaps: a class (courseIndex >= 0) or another event.
struct AutoClash {
    int courseIndex = -1;    // class affected, -1 when the clash is another event
    int eventIndex = -1;     // other event, -1 when the clash is a class
    int day = 0;
    int startMin = 0;        // time of the affected class / event
    int endMin = 0;
    int overlapMin = 0;      // minutes shared with the placed event
};

struct AutoEventPlacement {
    int eventIndex = 0;
    bool placed = false;
    int day = 0;
    int startMin = 0;
    int endMin = 0;
    bool forced = false;     // no free slot was long enough, fallback placement
    bool shortened = false;  // event is longer than the teaching day and was cut
    std::vector<AutoClash> clashes;   // empty when the event sits in free time
};

struct AutoEventResult {
    bool ok = true;                              // false only when an event could not be placed at all
    std::string message;
    int clashingEvents = 0;                      // events that overlap a class or another event
    std::vector<AutoEventPlacement> placements;  // one entry per input event, same order
};

class AutoScheduler {
public:
    // Number of whole slots that fit between dayStartMin and dayEndMin.
    static int slotsPerDay(const AutoPrefs& prefs);
    // Minutes a class of `slots` consecutive slots occupies (including inner breaks).
    static int sessionMinutes(int slots, const AutoPrefs& prefs);
    // Start time (minutes since midnight) of slot number `slot`.
    static int slotStart(int slot, const AutoPrefs& prefs);

    static AutoResult schedule(const std::vector<AutoCourse>& courses, const AutoPrefs& prefs);

    // Put events into the free time left by `sessions`. Days with the most free
    // time are tried first. Events that fit nowhere are placed where they
    // overlap the least and the overlapped classes are listed.
    static AutoEventResult placeEvents(const std::vector<AutoEvent>& events,
                                       const std::vector<AutoSession>& sessions,
                                       const AutoPrefs& prefs);
};