#include "Domain.h"

using namespace std;

int World::addPlanet(const std::string& name) {
    planets.push_back(name);
    adj.push_back({});
    return static_cast<int>(planets.size()) - 1;
}

void World::addRoute(int a, int b, int fuel, double risk, bool bidir) {
    Route r1{ a, b, fuel, risk, bidir };
    adj[a].push_back(r1);
    if (bidir) {
        Route r2{ b, a, fuel, risk, bidir };
        adj[b].push_back(r2);
    }
}

RiskBucket World::bucket(double r) const {
    if (r <= Config::RISK_LOW_MAX) return RiskBucket::Low;
    if (r <= Config::RISK_MED_MAX) return RiskBucket::Medium;
    return RiskBucket::High;
}

const std::string& World::getPlanetName(int id) const {
    return planets.at(id);
}

int World::getPlanetCount() const {
    return static_cast<int>(planets.size());
}

const std::vector<Route>& World::getRoutesFrom(int planetId) const {
    return adj.at(planetId);
}
