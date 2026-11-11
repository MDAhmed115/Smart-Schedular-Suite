
#include "engine/AutoScheduler.h"
#include <algorithm>
#include <climits>
#include <numeric>
#include <random>
#include <string>
#include <utility>

namespace {

struct Job {
    int course = 0;
    int slots = 1;
    int buffer = 0;   // free slots wanted after this class (a "long break")
};

struct Block {
    int s = 0, e = 0, buf = 0;   // slot range [s, e) plus trailing buffer
};

struct State {
    const AutoPrefs* prefs = nullptr;
    int slotsPerDay = 0;
    std::vector<Job> jobs;
    std::vector<std::vector<Block>> busy;         // per day
    std::vector<std::vector<bool>> courseOnDay;   // [course][day]
    std::vector<AutoSession> placed;
    bool strictDays = true;                       // same course on different days
    bool useBuffers = true;
    long long nodes = 0;
    long long nodeLimit = 400000;
    std::mt19937 rng;
};

bool fits(const State& st, int day, int s, int e, int buf) {
    for (const Block& b : st.busy[day]) {
        if (s < b.e + b.buf && b.s < e + buf) return false;
    }
    return true;
}

bool place(State& st, std::size_t k) {
    if (k == st.jobs.size()) return true;
    if (++st.nodes > st.nodeLimit) return false;

    const Job& job = st.jobs[k];
    const int buf = st.useBuffers ? job.buffer : 0;

    std::vector<int> days;
    for (int d = 0; d < 7; ++d) {
        if (!st.prefs->dayAllowed[d]) continue;
        if (st.strictDays && st.courseOnDay[job.course][d]) continue;
        days.push_back(d);
    }
    // random order first, then lightest day first -> spread over the week
    std::shuffle(days.begin(), days.end(), st.rng);
    std::stable_sort(days.begin(), days.end(), [&](int a, int b) {
        return st.busy[a].size() < st.busy[b].size();
    });

    std::vector<int> starts;
    for (int s = 0; s + job.slots <= st.slotsPerDay; ++s) starts.push_back(s);

    for (int d : days) {
        std::shuffle(starts.begin(), starts.end(), st.rng);
        for (int s : starts) {
            const int e = s + job.slots;
            if (!fits(st, d, s, e, buf)) continue;

            const bool had = st.courseOnDay[job.course][d];
            st.busy[d].push_back({s, e, buf});
            st.courseOnDay[job.course][d] = true;

            AutoSession as;
            as.courseIndex = job.course;
            as.day = d;
            as.startSlot = s;
            as.slotCount = job.slots;
            as.startMin = AutoScheduler::slotStart(s, *st.prefs);
            as.endMin = AutoScheduler::slotStart(e - 1, *st.prefs) + st.prefs->slotMin;
            st.placed.push_back(as);

            if (place(st, k + 1)) return true;

            st.placed.pop_back();
            st.courseOnDay[job.course][d] = had;
            st.busy[d].pop_back();
            if (st.nodes > st.nodeLimit) return false;
        }
    }
    return false;
}

} // namespace

int AutoScheduler::slotsPerDay(const AutoPrefs& p) {
    if (p.slotMin < 1) return 0;
    const int span = p.dayEndMin - p.dayStartMin;
    if (span < p.slotMin) return 0;
    const int gap = p.slotBreakMin < 0 ? 0 : p.slotBreakMin;
    return (span + gap) / (p.slotMin + gap);
}

int AutoScheduler::sessionMinutes(int slots, const AutoPrefs& p) {
    if (slots < 1) return 0;
    const int gap = p.slotBreakMin < 0 ? 0 : p.slotBreakMin;
    return slots * p.slotMin + (slots - 1) * gap;
}

int AutoScheduler::slotStart(int slot, const AutoPrefs& p) {
    const int gap = p.slotBreakMin < 0 ? 0 : p.slotBreakMin;
    return p.dayStartMin + slot * (p.slotMin + gap);
}

AutoResult AutoScheduler::schedule(const std::vector<AutoCourse>& courses, const AutoPrefs& prefsIn) {
    AutoResult result;
    AutoPrefs prefs = prefsIn;
    if (prefs.slotBreakMin < 0) prefs.slotBreakMin = 0;

    if (courses.empty()) { result.message = "Add at least one course first."; return result; }
    if (prefs.slotMin < 1) { result.message = "One slot must last at least 1 minute."; return result; }
    if (prefs.dayEndMin <= prefs.dayStartMin) { result.message = "The day must end after it starts."; return result; }

    const int spd = slotsPerDay(prefs);
    result.slotsPerDay = spd;
    if (spd == 0) { result.message = "The teaching day is shorter than a single slot."; return result; }

    int allowedDays = 0;
    for (int d = 0; d < 7; ++d) if (prefs.dayAllowed[d]) ++allowedDays;
    if (allowedDays == 0) { result.message = "No days are available for classes."; return result; }

    std::mt19937 rng(prefs.seed ? prefs.seed : std::random_device{}());

    // How many free slots a "long break" is worth, given the requested range.
    const int period = prefs.slotMin + prefs.slotBreakMin;
    int lo = (prefs.longBreakMinMin - prefs.slotBreakMin + period - 1) / period;
    int hi = (prefs.longBreakMaxMin - prefs.slotBreakMin) / period;
    if (lo < 1) lo = 1;
    if (hi < lo) hi = lo;

    State st;
    st.prefs = &prefs;
    st.slotsPerDay = spd;
    st.rng = rng;

    long long totalSlots = 0;
    for (std::size_t c = 0; c < courses.size(); ++c) {
        const AutoCourse& ac = courses[c];
        if (ac.sessionsPerWeek < 1 || ac.slotsPerSession < 1) {
            result.message = "Course " + ac.code + " needs at least 1 session of at least 1 slot.";
            return result;
        }
        if (ac.slotsPerSession > spd) {
            result.message = "A session of " + ac.code + " needs " + std::to_string(ac.slotsPerSession)
                + " slots but a day only has " + std::to_string(spd) + ".";
            return result;
        }
        for (int i = 0; i < ac.sessionsPerWeek; ++i) {
            Job j;
            j.course = static_cast<int>(c);
            j.slots = ac.slotsPerSession;
            if (prefs.longBreaks && std::uniform_int_distribution<int>(0, 99)(st.rng) < 55)
                j.buffer = std::uniform_int_distribution<int>(lo, hi)(st.rng);
            st.jobs.push_back(j);
        }
        totalSlots += static_cast<long long>(ac.slotsPerSession) * ac.sessionsPerWeek;
    }

    const long long capacity = static_cast<long long>(allowedDays) * spd;
    if (totalSlots > capacity) {
        result.message = "Not enough room: the courses need " + std::to_string(totalSlots)
            + " slots per week but only " + std::to_string(capacity) + " are available ("
            + std::to_string(spd) + " per day x " + std::to_string(allowedDays) + " days). "
            + "Reduce sessions or free up another day.";
        return result;
    }

    // Longest classes first: they are the hardest to fit.
    std::stable_sort(st.jobs.begin(), st.jobs.end(), [](const Job& a, const Job& b) { return a.slots > b.slots; });

    bool found = false;
    int usedPass = 0;
    for (int pass = 0; pass < 3 && !found; ++pass) {
        st.useBuffers = (pass == 0);
        st.strictDays = (pass <= 1);
        st.busy.assign(7, {});
        st.courseOnDay.assign(courses.size(), std::vector<bool>(7, false));
        st.placed.clear();
        st.nodes = 0;
        found = place(st, 0);
        usedPass = pass;
    }

    if (!found) {
        result.message = "Could not fit every session without overlap. Try fewer or shorter classes, a longer day, or no reserve day.";
        return result;
    }

    result.sessions = st.placed;
    std::sort(result.sessions.begin(), result.sessions.end(), [](const AutoSession& a, const AutoSession& b) {
        return a.day != b.day ? a.day < b.day : a.startMin < b.startMin;
    });
    result.ok = true;
    result.longBreaksApplied = prefs.longBreaks && usedPass == 0;
    result.message = "Timetable generated: " + std::to_string(result.sessions.size()) + " sessions, no overlaps.";
    if (prefs.longBreaks && usedPass > 0)
        result.message += " The week was too full for longer breaks, so classes were packed more tightly.";
    return result;
}

AutoEventResult AutoScheduler::placeEvents(const std::vector<AutoEvent>& events,
                                           const std::vector<AutoSession>& sessions,
                                           const AutoPrefs& prefs) {
    AutoEventResult result;
    result.placements.resize(events.size());
    for (std::size_t i = 0; i < events.size(); ++i) result.placements[i].eventIndex = static_cast<int>(i);
    if (events.empty()) { result.message = "No events yet."; return result; }

    const int dayStart = prefs.dayStartMin;
    const int dayEnd = prefs.dayEndMin;
    const int margin = prefs.slotBreakMin < 0 ? 0 : prefs.slotBreakMin;
    const int step = 5;

    struct Item { int start = 0, end = 0, course = -1, event = -1; };
    std::vector<std::vector<Item>> busy(7);
    for (const AutoSession& s : sessions)
        if (s.day >= 0 && s.day < 7) busy[s.day].push_back({s.startMin, s.endMin, s.courseIndex, -1});

    auto freeMinutes = [&](int d) {
        long long used = 0;
        for (auto& b : busy[d]) used += std::max(0, std::min(b.end, dayEnd) - std::max(b.start, dayStart));
        return static_cast<long long>(dayEnd - dayStart) - used;
    };
    auto fitsAt = [&](int d, int start, int end) {
        for (auto& b : busy[d])
            if (start < b.end + margin && b.start < end + margin) return false;
        return true;
    };
    auto tryDay = [&](int d, int minutes, int eventIdx, AutoEventPlacement& out) {
        for (int t = dayStart; t + minutes <= dayEnd; t += step) {
            if (fitsAt(d, t, t + minutes)) {
                out.placed = true; out.day = d; out.startMin = t; out.endMin = t + minutes;
                busy[d].push_back({t, t + minutes, -1, eventIdx});
                return true;
            }
        }
        return false;
    };
    // Fixed-time events are committed first. Their requested times are never
    // shifted or shortened; conflicts are reported while preserving the time.
    for (std::size_t idx = 0; idx < events.size(); ++idx) {
        const AutoEvent& ev = events[idx];
        if (!ev.customTime) continue;
        AutoEventPlacement& out = result.placements[idx];
        if (ev.preferredDay < 0 || ev.preferredDay >= 7 ||
            ev.customStartMin < 0 || ev.customEndMin <= ev.customStartMin || ev.customEndMin > 24 * 60)
            continue;

        out.placed = true;
        out.day = ev.preferredDay;
        out.startMin = ev.customStartMin;
        out.endMin = ev.customEndMin;
        for (const Item& b : busy[out.day]) {
            const int overlapStart = std::max(b.start, out.startMin);
            const int overlapEnd = std::min(b.end, out.endMin);
            if (overlapEnd <= overlapStart) continue;
            AutoClash clash;
            clash.courseIndex = b.course;
            clash.eventIndex = b.event;
            clash.day = out.day;
            clash.startMin = b.start;
            clash.endMin = b.end;
            clash.overlapMin = overlapEnd - overlapStart;
            out.clashes.push_back(clash);
        }
        if (!out.clashes.empty()) ++result.clashingEvents;
        busy[out.day].push_back({out.startMin, out.endMin, -1, static_cast<int>(idx)});
    }
    // Preferred day first, then the days with the most free time.
    auto dayOrder = [&](const AutoEvent& ev) {
        std::vector<int> days;
        if (ev.preferredDay >= 0 && ev.preferredDay < 7 && prefs.dayAllowed[ev.preferredDay])
            days.push_back(ev.preferredDay);
        std::vector<int> others;
        for (int d = 0; d < 7; ++d)
            if (prefs.dayAllowed[d] && d != ev.preferredDay) others.push_back(d);
        std::stable_sort(others.begin(), others.end(), [&](int a, int b) { return freeMinutes(a) > freeMinutes(b); });
        days.insert(days.end(), others.begin(), others.end());
        return days;
    };

    // Longest events first; they are the hardest to fit.
    std::vector<int> order;
    for (std::size_t i = 0; i < events.size(); ++i)
        if (!events[i].customTime) order.push_back(static_cast<int>(i));
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return events[a].minutes > events[b].minutes; });

    // Pass 1: free time only.
    int placedCount = 0;
    for (const AutoEventPlacement& placement : result.placements)
        if (placement.placed) ++placedCount;
    std::vector<int> leftover;
    for (int idx : order) {
        const AutoEvent& ev = events[idx];
        AutoEventPlacement& out = result.placements[idx];
        if (ev.minutes < 1) continue;
        bool done = false;
        for (int d : dayOrder(ev))
            if (tryDay(d, ev.minutes, idx, out)) { ++placedCount; done = true; break; }
        if (!done) leftover.push_back(idx);
    }

    // Pass 2: nothing was free for these. Put each one where it overlaps the
    // least and record what it overlaps. Other events count three times as
    // much as classes so events avoid stacking on each other.
    const int span = dayEnd - dayStart;
    for (int idx : leftover) {
        const AutoEvent& ev = events[idx];
        AutoEventPlacement& out = result.placements[idx];
        const int len = std::min(ev.minutes, span);
        if (len < 1) continue;

        long long bestCost = LLONG_MAX;
        int bestDay = -1, bestStart = 0;
        for (int d : dayOrder(ev)) {
            for (int t = dayStart; t + len <= dayEnd; t += step) {
                long long cost = 0;
                for (const Item& b : busy[d]) {
                    const int ov = std::min(b.end, t + len) - std::max(b.start, t);
                    if (ov > 0) cost += (b.event >= 0 ? 3LL : 1LL) * ov + 10;   // +10: fewer things hit is better
                }
                if (cost < bestCost) { bestCost = cost; bestDay = d; bestStart = t; }
            }
        }
        if (bestDay < 0) continue;

        out.placed = true;
        out.forced = true;
        out.shortened = len < ev.minutes;
        out.day = bestDay;
        out.startMin = bestStart;
        out.endMin = bestStart + len;
        for (const Item& b : busy[bestDay]) {
            const int ov = std::min(b.end, out.endMin) - std::max(b.start, out.startMin);
            if (ov <= 0) continue;
            AutoClash c;
            c.courseIndex = b.course;
            c.eventIndex = b.event;
            c.day = bestDay;
            c.startMin = b.start;
            c.endMin = b.end;
            c.overlapMin = ov;
            out.clashes.push_back(c);
        }
        busy[bestDay].push_back({out.startMin, out.endMin, -1, idx});
        ++placedCount;
        if (!out.clashes.empty()) ++result.clashingEvents;
    }

    result.ok = placedCount == static_cast<int>(events.size());
    result.message = "Placed " + std::to_string(placedCount) + " of " + std::to_string(events.size()) + " events.";
    if (result.clashingEvents > 0)
        result.message += " " + std::to_string(result.clashingEvents)
            + (result.clashingEvents == 1 ? " event has" : " events have")
            + " a time conflict; the affected classes or events are listed.";
    if (!result.ok) result.message += " Some events have no length and were skipped.";
    return result;
}
