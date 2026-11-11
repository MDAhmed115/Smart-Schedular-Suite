#pragma once
#include <string>
#include <vector>

// Study planner engine. Qt-free. It never reads the other modules directly:
// the GUI hands it a snapshot of the weekly timetable and event planners.
//
// Rules it follows for every calendar day between today and the last exam:
//   1. University classes / university events decide when the student is free.
//      Study starts `breakAfterUniMin` (default 1 h) after the last one ends.
//   2. External events are blocked out (with a margin) afterwards.
//   3. Meals are blocked, study is cut into focus blocks with breaks, and a
//      daily limit keeps the day realistic (nobody studies from dawn to midnight).
//   4. Harder courses and nearer exams get more time; the day before an exam
//      is used for that course.
//   5. Exam day: there are no classes or university events. The student still
//      studies: before the exam (the exam course first) and after it (for the
//      next exam), around the exam's own start and end time.

struct StudyCourse {
    std::string code;
    std::string name;
    int difficulty = 5;        // 1..10
    long long examJd = 0;      // Julian day number of the exam date
    int examStartMin = 9 * 60; // exam start, minutes since midnight
    int examEndMin = 12 * 60;  // exam end
};

enum class BusyKind { Class, UniEvent, ExternalEvent };

// One commitment. weekday: 0 = Monday ... 6 = Sunday.
// jd < 0  : repeats every week on `weekday` (classes).
// jd >= 0 : happens only on that exact calendar day (Julian day number);
//           used for events so one-off, daily, weekly and monthly events
//           all land on the right dates.
struct StudyBusy {
    int weekday = 0;
    int startMin = 0;
    int endMin = 0;
    BusyKind kind = BusyKind::Class;
    std::string label;
    long long jd = -1;
};

struct StudyPrefs {
    int freeDayStartMin = 9 * 60;     // first study time on a day without classes
    int latestEndMin = 22 * 60 + 30;  // no studying after this
    int breakAfterUniMin = 60;        // rest between the last class and the first study block
    int focusMin = 50;                // length of one study block
    int shortBreakMin = 10;
    int longBreakMin = 20;            // after every second block
    int minBlockMin = 30;
    int dinnerStartMin = 20 * 60;
    int dinnerLenMin = 60;
    int lunchStartMin = 13 * 60;
    int lunchLenMin = 60;
    int externalMarginMin = 15;       // buffer around external events
    int maxWeekdayStudyMin = 240;     // days with classes
    int maxFreeDayStudyMin = 420;     // days without classes
    int maxExamDayStudyMin = 300;     // exam days
    int examBufferBeforeMin = 30;     // stop studying this long before an exam starts
    int breakAfterExamMin = 45;       // rest after an exam before studying again
    int restWeekday = -1;             // -1 = none; the day is kept free unless an exam is near
    bool useCampusGaps = true;        // use long free periods between classes
    int minCampusGapMin = 75;
};

enum class BlockKind { Study, ExamEve, Revision, ExamDay };

struct StudyBlock {
    int course = 0;                   // index into the input course vector
    int startMin = 0;
    int endMin = 0;
    BlockKind kind = BlockKind::Study;
    bool onCampus = false;            // placed in a gap between classes
};

struct StudyExam {
    int course = 0;                   // index into the input course vector
    int startMin = 0;
    int endMin = 0;
};

struct StudyDay {
    long long jd = 0;
    int weekday = 0;
    int uniEndMin = -1;               // end of the last class / university event, -1 = none
    int studyFromMin = -1;            // start of the first study block, -1 = no study
    bool restDay = false;
    std::vector<StudyBusy> busy;      // everything fixed on this day
    std::vector<StudyBlock> blocks;   // chronological
    std::vector<int> examCourses;     // exams happening this day
    std::vector<StudyExam> exams;     // the same exams with their times, sorted by start
    int studyMinutes() const {
        int t = 0;
        for (const StudyBlock& b : blocks) t += b.endMin - b.startMin;
        return t;
    }
};

struct StudyPlan {
    bool ok = false;
    std::string message;
    std::vector<std::string> warnings;
    std::vector<StudyDay> days;       // days[0] = today
    std::vector<int> neededMin;       // per input course
    std::vector<int> plannedMin;      // per input course
    long long startJd = 0;
};

class StudyPlanner {
public:
    static StudyPlan plan(const std::vector<StudyCourse>& courses,
                          const std::vector<StudyBusy>& weekly,
                          const StudyPrefs& prefs,
                          long long todayJd,
                          int nowMin);
};