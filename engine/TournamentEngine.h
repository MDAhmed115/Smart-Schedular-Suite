#pragma once

#include "core/ds/DynArray.h"

#include <cstddef>
#include <string>

// Tournament module engine. Completely independent from AutoScheduler,
// EventPlanner and TimetableSolver. Qt-free: only the project's own DSA
// containers (DynArray, HashTable, mergeSort) are used.
//
// Formats
//   RoundRobinPlayoffs : league (every team plays every other team 1 or 2
//                        times) -> top 4 -> semi-finals -> final (+ 3rd place)
//   KnockoutGroups     : group stage (round robin inside each group) ->
//                        Round of 32 (top 2 of each group + best third-placed
//                        teams, FIFA 2026 style) -> R16 -> QF -> SF -> final
//   KnockoutDirect     : straight into a single-elimination bracket
//                        (Round of 32 for 17-32 teams; smaller fields use the
//                        next power of two and the top seeds get byes)

enum class TMode { RoundRobinPlayoffs, KnockoutGroups, KnockoutDirect };

// Order matters: everything from R32 onward is a knockout stage.
enum class TStage { League, Group, R32, R16, QF, SF, ThirdPlace, Final };

struct TConfig {
    TMode mode = TMode::RoundRobinPlayoffs;
    int legs = 1;            // 1 = single round robin, 2 = home and away
    bool thirdPlace = true;  // play a third-place match
    int groups = 12;         // KnockoutGroups only (11..16, 12 = World Cup 2026)
};

struct TMatch {
    int id = -1;
    TStage stage = TStage::League;
    int group = -1;          // group index (league = 0), -1 for knockouts
    int round = 1;           // matchday inside league/group
    int home = -1;           // team index, -1 = not known yet (TBD)
    int away = -1;
    bool bye = false;
    bool played = false;
    int homeGoals = 0;
    int awayGoals = 0;
    bool penalties = false;  // decided by shoot-out
    int homePen = 0;
    int awayPen = 0;
    int winner = -1;         // team index, -1 = draw (league) / undecided
    int nextMatch = -1;      // where the winner goes
    int nextSlot = 0;        // 0 = home, 1 = away
    int loserNext = -1;      // semi-final losers go to the third-place match
    int loserSlot = 0;
};

struct TStanding {
    int team = -1;
    int played = 0, won = 0, drawn = 0, lost = 0;
    int gf = 0, ga = 0, gd = 0, points = 0;
};

class Tournament {
    DynArray<std::string> names_;
    DynArray<int> teamGroup_;
    DynArray<TMatch> matches_;
    TConfig cfg_;
    int groupCount_ = 1;
    int bracketSize_ = 0;
    bool playoffsBuilt_ = false;
    bool ready_ = false;

    int addMatch(TStage stage, int group, int round, int home, int away);
    void addRoundRobin(const DynArray<int>& teams, int group, TStage stage);
    void buildBracket(const DynArray<int>& firstRoundSlots, int size);
    void buildPlayoffs();
    void propagate(int matchId);
    void sortRows(DynArray<TStanding>& rows) const;
    void checkPlayoffs();

public:
    // Validates the input and creates every fixture. Returns false and fills
    // `error` when the input cannot make a tournament.
    bool init(const DynArray<std::string>& names, const TConfig& cfg, std::string& error);

    // Records (or edits) a result. Knockout draws need a shoot-out winner
    // (homePen != awayPen). League results lock once the playoffs exist.
    bool setResult(int matchId, int homeGoals, int awayGoals, int homePen, int awayPen,
                   std::string& error);

    // Table of one group (league = group 0), sorted by points, goal
    // difference, goals scored, name.
    DynArray<TStanding> standings(int group) const;

    bool ready() const { return ready_; }
    const TConfig& config() const { return cfg_; }
    int teamCount() const { return static_cast<int>(names_.size()); }
    const std::string& teamName(int team) const { return names_[static_cast<std::size_t>(team)]; }
    int teamGroup(int team) const { return teamGroup_[static_cast<std::size_t>(team)]; }
    int groupCount() const { return groupCount_; }
    int matchCount() const { return static_cast<int>(matches_.size()); }
    const TMatch& match(int id) const { return matches_[static_cast<std::size_t>(id)]; }
    int bracketSize() const { return bracketSize_; }
    bool playoffsBuilt() const { return playoffsBuilt_; }
    bool leagueFinished() const;

    int champion() const;       // -1 until the final is played
    int runnerUp() const;
    int thirdPlaceTeam() const; // -1 when not played / disabled

    static bool playable(const TMatch& m) { return !m.bye && m.home >= 0 && m.away >= 0; }
    static bool isKnockout(TStage s) { return static_cast<int>(s) >= static_cast<int>(TStage::R32); }
    static const char* stageName(TStage s);
};