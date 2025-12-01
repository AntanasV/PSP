#pragma once

#include <vector>
#include <utility>
#include <cstdint>

#include "Domain.h"
#include "Events.h"

struct Game {
    World world;
    Player player;
    int rounds;
    std::vector<int> roundTargets; // for 1..5, last is Earth
    int earthId;

    // Planetų station naudotas ar ne
    std::vector<bool> stationVisited; // true: ši planeta jau tikrinta (operational/wreck/ambush)

    Game();

    double roundMult(int roundNo) const;
    int missionBonus(int roundNo) const;
    void line();
    void initWorld();
    void printStatus(int roundNo, int target);
    void printMap();
    void openShop(Player& pl);
    std::vector<std::pair<EventType, int>> buildEventPool(RiskBucket b, int roundNo);
    static EventType weightedPickDeterministic(const std::vector<std::pair<EventType, int>>& pool, std::uint64_t seed);
    EventType deterministicEventForRoute(const Route& r, int roundNo);
    void showNeighborsWithPreview(int roundNo, int target, std::vector<Route>& options);
    bool isStrandedHere() const;
    bool maybeVisitLocalStation(int roundNo);
    bool resolveHop(const Route& r, int roundNo, int target);
    bool runRound(int roundNo, int target);
    void run();
};
