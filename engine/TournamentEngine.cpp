#include "engine/TournamentEngine.h"

#include "core/ds/HashTable.h"
#include "core/algo/Sorting.h"

#include <utility>

namespace {

inline std::size_t U(int v) { return static_cast<std::size_t>(v); }
inline int I(std::size_t v) { return static_cast<int>(v); }

int nextPow2(int n) {
    int s = 2;
    while (s < n) s *= 2;
    return s;
}

// Standard single-elimination seed order: 1 v S, and the top seeds can only
// meet late. S = 4 -> [1,4,2,3]; consecutive pairs are the first-round matches.
DynArray<int> seedOrder(int size) {
    DynArray<int> order;
    order.push_back(1);
    while (I(order.size()) < size) {
        const int total = I(order.size()) * 2;
        DynArray<int> next;
        for (std::size_t i = 0; i < order.size(); ++i) {
            next.push_back(order[i]);
            next.push_back(total + 1 - order[i]);
        }
        order = next;
    }
    return order;
}

TStage stageForTeams(int teamsInRound) {
    switch (teamsInRound) {
        case 32: return TStage::R32;
        case 16: return TStage::R16;
        case 8:  return TStage::QF;
        case 4:  return TStage::SF;
        default: return TStage::Final;
    }
}

// teamsBySeed[0] is seed 1. Seeds beyond the field become byes (-1).
DynArray<int> slotsFromSeeds(const DynArray<int>& teamsBySeed, int size) {
    DynArray<int> order = seedOrder(size);
    DynArray<int> slots;
    for (std::size_t i = 0; i < order.size(); ++i) {
        const int seed = order[i];
        slots.push_back(seed <= I(teamsBySeed.size()) ? teamsBySeed[U(seed - 1)] : -1);
    }
    return slots;
}

} // namespace

const char* Tournament::stageName(TStage s) {
    switch (s) {
        case TStage::League:     return "League";
        case TStage::Group:      return "Group stage";
        case TStage::R32:        return "Round of 32";
        case TStage::R16:        return "Round of 16";
        case TStage::QF:         return "Quarter-finals";
        case TStage::SF:         return "Semi-finals";
        case TStage::ThirdPlace: return "Third place";
        case TStage::Final:      return "Final";
    }
    return "";
}

int Tournament::addMatch(TStage stage, int group, int round, int home, int away) {
    TMatch m;
    m.id = I(matches_.size());
    m.stage = stage;
    m.group = group;
    m.round = round;
    m.home = home;
    m.away = away;
    matches_.push_back(m);
    return m.id;
}

// Circle method. Every pair meets once per leg; leg 2 swaps home and away.
void Tournament::addRoundRobin(const DynArray<int>& teams, int group, TStage stage) {
    DynArray<int> base = teams;
    if (base.size() % 2 == 1) base.push_back(-1);  // -1 = rest slot
    const int m = I(base.size());
    const int rounds = m - 1;

    for (int leg = 0; leg < cfg_.legs; ++leg) {
        DynArray<int> cur = base;
        for (int r = 0; r < rounds; ++r) {
            for (int i = 0; i < m / 2; ++i) {
                int a = cur[U(i)], b = cur[U(m - 1 - i)];
                if (a < 0 || b < 0) continue;
                if (i == 0 && (r % 2 == 1)) std::swap(a, b);  // balance home games
                if (leg == 1) std::swap(a, b);
                addMatch(stage, group, leg * rounds + r + 1, a, b);
            }
            const int last = cur[U(m - 1)];
            for (int j = m - 1; j > 1; --j) cur[U(j)] = cur[U(j - 1)];
            cur[1] = last;
        }
    }
}

void Tournament::propagate(int matchId) {
    const TMatch& m = matches_[U(matchId)];
    if (m.winner < 0 || m.nextMatch < 0) return;
    TMatch& next = matches_[U(m.nextMatch)];
    if (m.nextSlot == 0) next.home = m.winner;
    else next.away = m.winner;
}

void Tournament::buildBracket(const DynArray<int>& slots, int size) {
    DynArray<int> starts;
    DynArray<int> counts;

    for (int t = size; t >= 2; t /= 2) {
        starts.push_back(static_cast<int>(matches_.size()));
        counts.push_back(t / 2);
        const TStage st = stageForTeams(t);
        for (int p = 0; p < t / 2; ++p) addMatch(st, -1, 1, -1, -1);
    }

    const int rounds = I(starts.size());
    for (int r = 0; r + 1 < rounds; ++r) {
        for (int p = 0; p < counts[U(r)]; ++p) {
            TMatch& m = matches_[U(starts[U(r)] + p)];
            m.nextMatch = starts[U(r + 1)] + p / 2;
            m.nextSlot = p % 2;
        }
    }

    if (cfg_.thirdPlace && size >= 4) {
        const int tp = addMatch(TStage::ThirdPlace, -1, 1, -1, -1);
        const int semi = starts[U(rounds - 2)];
        for (int p = 0; p < 2; ++p) {
            matches_[U(semi + p)].loserNext = tp;
            matches_[U(semi + p)].loserSlot = p;
        }
    }

    for (int p = 0; p < counts[0]; ++p) {
        const int id = starts[0] + p;
        matches_[U(id)].home = slots[U(2 * p)];
        matches_[U(id)].away = slots[U(2 * p + 1)];
    }

    // Byes: a team without an opponent advances automatically.
    for (int p = 0; p < counts[0]; ++p) {
        const int id = starts[0] + p;
        TMatch& m = matches_[U(id)];
        if (m.home >= 0 && m.away < 0) {
            m.bye = true; m.played = true; m.winner = m.home;
            propagate(id);
        } else if (m.away >= 0 && m.home < 0) {
            m.bye = true; m.played = true; m.winner = m.away;
            propagate(id);
        }
    }

    bracketSize_ = size;
    playoffsBuilt_ = true;
}

void Tournament::sortRows(DynArray<TStanding>& rows) const {
    mergeSort(rows, [this](const TStanding& a, const TStanding& b) {
        if (a.points != b.points) return a.points > b.points;
        if (a.gd != b.gd) return a.gd > b.gd;
        if (a.gf != b.gf) return a.gf > b.gf;
        const std::string& x = names_[U(a.team)];
        const std::string& y = names_[U(b.team)];
        if (x != y) return x < y;
        return a.team < b.team;
    });
}

DynArray<TStanding> Tournament::standings(int group) const {
    DynArray<TStanding> rows;
    DynArray<int> pos;
    const int n = I(names_.size());
    for (int t = 0; t < n; ++t) {
        if (teamGroup_[U(t)] == group) {
            pos.push_back(I(rows.size()));
            TStanding s;
            s.team = t;
            rows.push_back(s);
        } else {
            pos.push_back(-1);
        }
    }

    for (std::size_t i = 0; i < matches_.size(); ++i) {
        const TMatch& m = matches_[i];
        if ((m.stage != TStage::League && m.stage != TStage::Group) || m.group != group || !m.played)
            continue;
        TStanding& h = rows[U(pos[U(m.home)])];
        TStanding& a = rows[U(pos[U(m.away)])];
        h.played++; a.played++;
        h.gf += m.homeGoals; h.ga += m.awayGoals;
        a.gf += m.awayGoals; a.ga += m.homeGoals;
        if (m.homeGoals > m.awayGoals) { h.won++; a.lost++; h.points += 3; }
        else if (m.homeGoals < m.awayGoals) { a.won++; h.lost++; a.points += 3; }
        else { h.drawn++; a.drawn++; h.points++; a.points++; }
    }
    for (std::size_t i = 0; i < rows.size(); ++i) rows[i].gd = rows[i].gf - rows[i].ga;

    sortRows(rows);
    return rows;
}

bool Tournament::leagueFinished() const {
    bool any = false;
    for (std::size_t i = 0; i < matches_.size(); ++i) {
        const TMatch& m = matches_[i];
        if (m.stage != TStage::League && m.stage != TStage::Group) continue;
        any = true;
        if (!m.played) return false;
    }
    return any;
}

void Tournament::checkPlayoffs() {
    if (playoffsBuilt_ || cfg_.mode == TMode::KnockoutDirect) return;
    if (leagueFinished()) buildPlayoffs();
}

void Tournament::buildPlayoffs() {
    if (cfg_.mode == TMode::RoundRobinPlayoffs) {
        DynArray<TStanding> rows = standings(0);
        DynArray<int> top;
        for (int i = 0; i < 4; ++i) top.push_back(rows[U(i)].team);
        buildBracket(slotsFromSeeds(top, 4), 4);
        return;
    }

    // FIFA 2026 style: group winners, runners-up and the best third-placed
    // teams fill the 32 places. Seeds: winners, then runners-up, then thirds.
    DynArray<TStanding> winners, runners, thirds;
    for (int g = 0; g < groupCount_; ++g) {
        DynArray<TStanding> rows = standings(g);
        winners.push_back(rows[0]);
        runners.push_back(rows[1]);
        thirds.push_back(rows[2]);
    }
    sortRows(winners);
    sortRows(runners);
    sortRows(thirds);

    DynArray<int> seeded;
    for (std::size_t i = 0; i < winners.size(); ++i) seeded.push_back(winners[i].team);
    for (std::size_t i = 0; i < runners.size(); ++i) seeded.push_back(runners[i].team);
    const int needThirds = 32 - 2 * groupCount_;
    for (int i = 0; i < needThirds; ++i) seeded.push_back(thirds[U(i)].team);

    DynArray<int> slots = slotsFromSeeds(seeded, 32);

    // Avoid Round of 32 matches between teams of the same group.
    auto grp = [this](int t) { return t < 0 ? -1 : teamGroup_[U(t)]; };
    for (int p = 0; p < 16; ++p) {
        const int a = slots[U(2 * p)], b = slots[U(2 * p + 1)];
        if (a < 0 || b < 0 || grp(a) != grp(b)) continue;
        for (int k = 1; k < 16; ++k) {
            const int q = (p + k) % 16;
            const int c = slots[U(2 * q + 1)];
            if (grp(a) != grp(c) && grp(slots[U(2 * q)]) != grp(b)) {
                slots[U(2 * p + 1)] = c;
                slots[U(2 * q + 1)] = b;
                break;
            }
        }
    }
    buildBracket(slots, 32);
}

bool Tournament::init(const DynArray<std::string>& names, const TConfig& cfg, std::string& error) {
    *this = Tournament();
    const int n = I(names.size());

    HashTable<std::string, int, StringHash> seen;
    for (int i = 0; i < n; ++i) {
        if (names[U(i)].empty()) { error = "A team name is empty."; return false; }
        if (!seen.insert(names[U(i)], i)) {
            error = "Duplicate team name: " + names[U(i)];
            return false;
        }
    }
    if (cfg.legs != 1 && cfg.legs != 2) { error = "Teams can meet 1 or 2 times."; return false; }

    names_ = names;
    cfg_ = cfg;
    for (int i = 0; i < n; ++i) teamGroup_.push_back(0);

    if (cfg.mode == TMode::RoundRobinPlayoffs) {
        if (n < 4) { error = "Round robin with semi-finals needs at least 4 teams."; *this = Tournament(); return false; }
        if (n > 64) { error = "At most 64 teams are supported."; *this = Tournament(); return false; }
        groupCount_ = 1;
        DynArray<int> all;
        for (int i = 0; i < n; ++i) all.push_back(i);
        addRoundRobin(all, 0, TStage::League);
    } else if (cfg.mode == TMode::KnockoutDirect) {
        if (n < 2 || n > 32) { error = "Direct knockout supports 2 to 32 teams (32 = full Round of 32)."; *this = Tournament(); return false; }
        groupCount_ = 1;
        DynArray<int> seeds;
        for (int i = 0; i < n; ++i) seeds.push_back(i);
        const int size = nextPow2(n);
        buildBracket(slotsFromSeeds(seeds, size), size);
    } else {
        const int g = cfg.groups;
        if (g < 11 || g > 16) {
            error = "Groups must be between 11 and 16 so that 32 teams qualify (12 groups = World Cup 2026).";
            *this = Tournament();
            return false;
        }
        if (n < 3 * g) { error = "Need at least 3 teams per group (" + std::to_string(3 * g) + " teams for " + std::to_string(g) + " groups)."; *this = Tournament(); return false; }
        if (n > 64) { error = "At most 64 teams are supported."; *this = Tournament(); return false; }
        groupCount_ = g;
        // Teams are dealt into groups in input order (pot 1 first, like a draw).
        for (int i = 0; i < n; ++i) teamGroup_[U(i)] = i % g;
        for (int grpIdx = 0; grpIdx < g; ++grpIdx) {
            DynArray<int> members;
            for (int i = 0; i < n; ++i) if (teamGroup_[U(i)] == grpIdx) members.push_back(i);
            addRoundRobin(members, grpIdx, TStage::Group);
        }
    }

    ready_ = true;
    return true;
}

bool Tournament::setResult(int matchId, int hg, int ag, int hp, int ap, std::string& error) {
    if (matchId < 0 || matchId >= I(matches_.size())) { error = "Unknown match."; return false; }
    TMatch& m = matches_[U(matchId)];
    if (!playable(m)) { error = "Both teams must be known before a result can be entered."; return false; }
    if (hg < 0 || ag < 0) { error = "Goals cannot be negative."; return false; }

    const bool ko = isKnockout(m.stage);
    if (!ko && playoffsBuilt_) { error = "League results are locked once the playoffs have started."; return false; }

    if (ko && m.played) {
        if (m.nextMatch >= 0 && matches_[U(m.nextMatch)].played) { error = "The next round has already been played."; return false; }
        if (m.loserNext >= 0 && matches_[U(m.loserNext)].played) { error = "The third-place match has already been played."; return false; }
    }

    bool pens = false;
    if (ko && hg == ag) {
        if (hp < 0 || ap < 0 || hp == ap) { error = "A knockout draw needs a shoot-out winner (penalties cannot be equal)."; return false; }
        pens = true;
    }

    m.homeGoals = hg; m.awayGoals = ag;
    m.penalties = pens;
    m.homePen = pens ? hp : 0;
    m.awayPen = pens ? ap : 0;
    m.played = true;
    if (hg > ag) m.winner = m.home;
    else if (ag > hg) m.winner = m.away;
    else if (pens) m.winner = hp > ap ? m.home : m.away;
    else m.winner = -1;

    if (ko) {
        propagate(matchId);
        if (m.loserNext >= 0) {
            const int loser = (m.winner == m.home) ? m.away : m.home;
            TMatch& tp = matches_[U(m.loserNext)];
            if (m.loserSlot == 0) tp.home = loser; else tp.away = loser;
        }
    } else {
        checkPlayoffs();  // may append matches: do not touch `m` after this
    }
    return true;
}

int Tournament::champion() const {
    for (std::size_t i = 0; i < matches_.size(); ++i)
        if (matches_[i].stage == TStage::Final && matches_[i].played) return matches_[i].winner;
    return -1;
}

int Tournament::runnerUp() const {
    for (std::size_t i = 0; i < matches_.size(); ++i) {
        const TMatch& m = matches_[i];
        if (m.stage == TStage::Final && m.played && !m.bye)
            return m.winner == m.home ? m.away : m.home;
    }
    return -1;
}

int Tournament::thirdPlaceTeam() const {
    for (std::size_t i = 0; i < matches_.size(); ++i)
        if (matches_[i].stage == TStage::ThirdPlace && matches_[i].played) return matches_[i].winner;
    return -1;
}