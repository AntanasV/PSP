#include "Game.h"
#include "Config.h"
#include "Utils.h"
#include "Events.h"

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <functional>

using namespace std;

Game::Game()
    : rounds(Config::MAX_ROUNDS),
    earthId(-1) {
}

// Raundo sunkumo multiplikatorius (event suveikimo šansui)
double Game::roundMult(int roundNo) const {
    if (roundNo >= 1 && roundNo <= Config::MAX_ROUNDS)
        return Config::ROUND_MULT[roundNo];
    return 1.0;
}

// Round completion bonus
int Game::missionBonus(int roundNo) const {
    if (roundNo >= 1 && roundNo <= Config::MAX_ROUNDS)
        return Config::MISSION_BONUS[roundNo];
    return Config::MISSION_BONUS[1];
}

void Game::line() {
    cout << "------------------------------------------------------------\n";
}

// Inicijuoja pasaulį, maršrutus ir round'ų tikslus
void Game::initWorld() {
    int eden = world.addPlanet("Eden Prime");
    int cit = world.addPlanet("Citadel");
    int omega = world.addPlanet("Omega");
    int ill = world.addPlanet("Illium");
    int nov = world.addPlanet("Noveria");
    int fer = world.addPlanet("Feros");
    int hor = world.addPlanet("Horizon");
    int tuc = world.addPlanet("Tuchanka");
    int pal = world.addPlanet("Palaven");
    int the = world.addPlanet("Thessia");
    int earth = world.addPlanet("Earth");
    earthId = earth;

    world.addRoute(eden, cit, 80, 0.30);
    world.addRoute(eden, fer, 60, 0.20);
    world.addRoute(fer, cit, 70, 0.25);
    world.addRoute(fer, hor, 65, 0.35);
    world.addRoute(hor, omega, 110, 0.60);
    world.addRoute(omega, ill, 90, 0.40);
    world.addRoute(ill, cit, 85, 0.20);
    world.addRoute(ill, nov, 75, 0.30);
    world.addRoute(nov, cit, 70, 0.35);
    world.addRoute(cit, pal, 95, 0.25);
    world.addRoute(pal, tuc, 120, 0.50);
    world.addRoute(tuc, omega, 100, 0.45);
    world.addRoute(cit, the, 90, 0.25);
    world.addRoute(the, ill, 70, 0.20);
    world.addRoute(the, earth, 140, 0.60);
    world.addRoute(pal, earth, 130, 0.55);

    player.setCurrentPlanet(eden);
    player.setPreviousPlanet(-1);
    player.setFuel(Config::STARTING_FUEL);
    player.setCredits(Config::STARTING_CREDITS);
    player.resetCargo();

    vector<int> candidates = { the, nov, pal, ill, omega, fer, hor, tuc, cit };
    shuffle(candidates.begin(), candidates.end(), rng);
    roundTargets.clear();
    for (int i = 0; i < 4 && i < (int)candidates.size(); ++i)
        roundTargets.push_back(candidates[i]);
    roundTargets.push_back(earth);

    stationVisited.assign(world.getPlanetCount(), false);
}

// Išveda žaidėjo statusą
void Game::printStatus(int roundNo, int target) {
    line();
    cout << "ROUND " << roundNo << "/" << rounds << "\n";
    cout << "Current location: " << world.getPlanetName(player.getCurrentPlanet()) << "\n";
    cout << "Target this round: " << world.getPlanetName(target) << "\n";
    cout << "Fuel: " << player.getFuel() << "   Credits: " << player.getCredits()
        << "   Cargo: " << (player.hasCargo() ? "ON BOARD" : "LOST") << "\n";

    const Upgrades& up = player.getUpgrades();
    cout << "Upgrades: ";
    bool any = false;
    if (up.hasWeaponsCalibration()) { cout << "[Weapons Calib] "; any = true; }
    if (up.getKineticBarriersLevel() == 1) { cout << "[Barriers Mk I] "; any = true; }
    if (up.getKineticBarriersLevel() == 2) { cout << "[Barriers Mk II] "; any = true; }
    if (up.hasEdiTargeting()) { cout << "[EDI Targeting] "; any = true; }
    if (up.hasEngineOverdrive()) { cout << "[Engine Overdrive] "; any = true; }
    if (up.hasRouteScanner()) { cout << "[Route Scanner] "; any = true; }
    if (up.hasReaperScanner()) { cout << "[Reaper Scanner] "; any = true; }
    if (up.hasStormShield()) { cout << "[Storm Shield] "; any = true; }
    if (!any) cout << "none";
    cout << "\n";
    line();
}

// ASCII žemėlapis
void Game::printMap() {
    cout << R"(
[Eden Prime] -------------- [Feros] ----------------- [Horizon]          
       \                   /                                  \          
        \                 /            [Noveria]               \         
         \               /            /         \               \        
          \             /            /           \               \       
           \           /            /             \               \      
            \         /            /               \               \     
             [Citadel] -----------|---------------- [Illium] ---- [Omega]
             /       \             \               /               /     
            /         \             \             /               /      
           /           \             \           /               /       
          /             \             \         /               /        
         /               \             [Thessia]               /         
        /                 \                                   /          
       /                   \                                 /           
      /                     \                               /            
  [Earth] ------------- [Palaven] ---------------- [Tuchanka]

)";
}

// Parduotuvė
void Game::openShop(Player& pl) {
    while (true) {
        cout << "Relay Station: you may buy fuel or upgrades. 1 Fuel = 1 Credit.\n";
        cout << "Credits: " << pl.getCredits() << "   Fuel: " << pl.getFuel() << "\n";
        cout << "1) Buy fuel\n";
        cout << "2) Buy upgrades\n";
        cout << "3) Leave station\n";
        cout << "Select: ";
        int c; if (!(cin >> c)) { return; }
        if (c == 1) {
            cout << "How much fuel to buy? ";
            int amt; cin >> amt;
            if (amt <= 0) { cout << "Nothing purchased.\n"; continue; }
            if (amt > pl.getCredits()) { cout << "Not enough credits.\n"; continue; }
            pl.spendCredits(amt);
            pl.addFuel(amt);
            cout << "Purchased " << amt << " fuel. Now you have "
                << pl.getFuel() << " fuel, " << pl.getCredits() << " credits.\n";
        }
        else if (c == 2) {
            while (true) {
                cout << "UPGRADES (discounted prices):\n";
                cout << " 1) Weapons Calibration (+10% fight) - 600\n";
                cout << " 2) Kinetic Barriers Mk I (+7% fight, -50% storm fuel loss, -Reaper catch) - 450\n";
                cout << " 3) Kinetic Barriers Mk II (+12% fight,...% storm fuel loss, more -Reaper catch) - 900 (replaces Mk I)\n";
                cout << " 4) EDI Targeting (+5% fight) - 400\n";
                cout << " 5) Engine Overdrive (-30% flee fuel cost, +10% flee success) - 500\n";
                cout << " 6) Route Scanner (preview event chance on routes) - 350\n";
                cout << " 7) Reaper Scanner (shows Reaper catch chance) - 300\n";
                cout << " 8) Storm Shield (-25% storm fuel loss, -storm cargo loss chance) - 350\n";
                cout << " 9) Back\n";
                cout << "Select upgrade: ";
                int u; cin >> u;

                if (u == 9) break;

                Upgrades& mods = pl.getUpgrades();

                auto buy = [&](int cost, function<void(Upgrades&)> apply) {
                    if (pl.getCredits() < cost) {
                        cout << "Not enough credits.\n";
                        return;
                    }
                    pl.spendCredits(cost);
                    apply(mods);
                    cout << "Upgrade purchased. Remaining credits: " << pl.getCredits() << "\n";
                    };

                if (u == 1) {
                    if (mods.hasWeaponsCalibration()) { cout << "You already own this.\n"; continue; }
                    buy(600, [](Upgrades& m) { m.buyWeaponsCalibration(); });
                }
                else if (u == 2) {
                    if (mods.getKineticBarriersLevel() >= 1) {
                        cout << "You already have Barriers (Mk " << mods.getKineticBarriersLevel() << ").\n";
                        continue;
                    }
                    buy(450, [](Upgrades& m) { m.setKineticBarriersLevel(1); });
                }
                else if (u == 3) {
                    if (mods.getKineticBarriersLevel() == 2) {
                        cout << "You already have Mk II.\n";
                        continue;
                    }
                    buy(900, [](Upgrades& m) { m.setKineticBarriersLevel(2); });
                }
                else if (u == 4) {
                    if (mods.hasEdiTargeting()) { cout << "You already own EDI Targeting.\n"; continue; }
                    buy(400, [](Upgrades& m) { m.enableEdiTargeting(); });
                }
                else if (u == 5) {
                    if (mods.hasEngineOverdrive()) { cout << "You already own Engine Overdrive.\n"; continue; }
                    buy(500, [](Upgrades& m) { m.enableEngineOverdrive(); });
                }
                else if (u == 6) {
                    if (mods.hasRouteScanner()) { cout << "You already own Route Scanner.\n"; continue; }
                    buy(350, [](Upgrades& m) { m.enableRouteScanner(); });
                }
                else if (u == 7) {
                    if (mods.hasReaperScanner()) { cout << "You already own Reaper Scanner.\n"; continue; }
                    buy(300, [](Upgrades& m) { m.enableReaperScanner(); });
                }
                else if (u == 8) {
                    if (mods.hasStormShield()) { cout << "You already own Storm Shield.\n"; continue; }
                    buy(350, [](Upgrades& m) { m.enableStormShield(); });
                }
                else {
                    cout << "Invalid option.\n";
                }
            }
        }
        else if (c == 3) {
            cout << "Leaving station.\n";
            break;
        }
        else {
            cout << "Invalid option.\n";
        }
    }
}

// Event pool kūrimas pagal bucket ir raundą
std::vector<std::pair<EventType, int>> Game::buildEventPool(RiskBucket b, int roundNo) {
    std::vector<std::pair<EventType, int>> pool;
    int baseCerb = 0, baseReaper = 0, baseStorm = 0, baseNav = 0;
    switch (b) {
    case RiskBucket::Low:
        baseCerb = (int)std::round(Config::CERB_BASE_PROB_LOW * 100);
        baseReaper = (int)std::round(Config::REAPER_BASE_PROB_LOW * 100);
        baseStorm = (int)std::round(Config::STORM_BASE_PROB_LOW * 100);
        baseNav = (int)std::round(Config::NAV_FAIL_BASE_PROB_LOW * 100);
        break;
    case RiskBucket::Medium:
        baseCerb = (int)std::round(Config::CERB_BASE_PROB_MED * 100);
        baseReaper = (int)std::round(Config::REAPER_BASE_PROB_MED * 100);
        baseStorm = (int)std::round(Config::STORM_BASE_PROB_MED * 100);
        baseNav = (int)std::round(Config::NAV_FAIL_BASE_PROB_MED * 100);
        break;
    case RiskBucket::High:
        baseCerb = (int)std::round(Config::CERB_BASE_PROB_HIGH * 100);
        baseReaper = (int)std::round(Config::REAPER_BASE_PROB_HIGH * 100);
        baseStorm = (int)std::round(Config::STORM_BASE_PROB_HIGH * 100);
        baseNav = (int)std::round(Config::NAV_FAIL_BASE_PROB_HIGH * 100);
        break;
    }

    double rm = roundMult(roundNo);
    auto scale = [&](int x) { return (int)std::round(x * rm); };

    pool.push_back({ EventType::Cerberus,       scale(baseCerb) });
    pool.push_back({ EventType::Reaper,         scale(baseReaper) });
    pool.push_back({ EventType::Storm,          scale(baseStorm) });
    pool.push_back({ EventType::NavigationFail, scale(baseNav) });

    return pool;
}

// Deterministinis pasirinkimas iš pool pagal seed (naudojamas preview)
EventType Game::weightedPickDeterministic(const std::vector<std::pair<EventType, int>>& pool, std::uint64_t seed) {
    int total = 0;
    for (auto& p : pool) total += p.second;
    if (total <= 0) return EventType::None;

    std::mt19937_64 gen(seed);
    std::uniform_int_distribution<int> dist(1, total);
    int r = dist(gen);
    int acc = 0;
    for (auto& p : pool) {
        acc += p.second;
        if (r <= acc) return p.first;
    }
    return EventType::None;
}

// Maršruto event preview (deterministinis)
EventType Game::deterministicEventForRoute(const Route& r, int roundNo) {
    HopContext ctx;
    ctx.edgeFuelBase = r.fuelCost;
    ctx.routeRisk = r.risk;
    ctx.bucket = world.bucket(r.risk);

    auto pool = buildEventPool(ctx.bucket, roundNo);
    std::uint64_t seed = (std::uint64_t)r.from * 1315423911ull
        ^ (std::uint64_t)r.to * 2654435761ull
        ^ (std::uint64_t)roundNo * 889523592379ull;
    return weightedPickDeterministic(pool, seed);
}

// Parodo kaimynus su preview
void Game::showNeighborsWithPreview(int roundNo, int target, std::vector<Route>& options) {
    options.clear();
    int cur = player.getCurrentPlanet();
    const auto& edges = world.getRoutesFrom(cur);
    cout << "Available routes from " << world.getPlanetName(cur) << ":\n";

    int idx = 1;
    for (auto& r : edges) {
        options.push_back(r);
        cout << " " << idx << ") To " << world.getPlanetName(r.to)
            << " | Fuel cost: " << r.fuelCost
            << " | Risk: " << std::fixed << std::setprecision(2) << r.risk;

        if (player.getUpgrades().hasRouteScanner()) {
            EventType et = deterministicEventForRoute(r, roundNo);
            cout << " | Event: ";
            switch (et) {
            case EventType::Cerberus:       cout << "CERBERUS encounter"; break;
            case EventType::Reaper:         cout << "REAPER attention";  break;
            case EventType::Storm:          cout << "Space storm";       break;
            case EventType::NavigationFail: cout << "Navigation anomaly"; break;
            case EventType::None:           cout << "none";              break;
            }
        }
        cout << "\n";
        ++idx;
    }

    cout << " " << idx << ") Open menu (status/map/quit)\n";
}

// Ar žaidėjas čia įstrigęs (nėra kelio dėl kuro)
bool Game::isStrandedHere() const {
    int cur = player.getCurrentPlanet();
    const auto& edges = world.getRoutesFrom(cur);
    for (auto& r : edges) {
        if (player.getFuel() >= r.fuelCost) return false;
    }
    return true;
}

// Galimybė aplankyti stotį
bool Game::maybeVisitLocalStation(int roundNo) {
    int p = player.getCurrentPlanet();
    if (p < 0 || p >= (int)stationVisited.size()) return true;
    if (stationVisited[p]) return true;

    cout << "[RELAY STATION] Local fuel/upgrade depot on " << world.getPlanetName(p) << ". Visit?\n";
    cout << "1) Yes  2) No\n";
    int c; if (!(cin >> c)) return false;
    if (c != 1) {
        cout << "You ignore the local station for now.\n";
        pauseMsg(Config::UI_PAUSE_MS);
        return true;
    }

    stationVisited[p] = true;
    cout << "You approach the relay station on " << world.getPlanetName(p) << ".\n";
    pauseMsg(Config::UI_PAUSE_MS);

    openShop(player);

    HopContext dummy;
    dummy.edgeFuelBase = 0;
    dummy.routeRisk = 0.4;
    dummy.bucket = RiskBucket::Medium;
    RoundConfig rcFake{ roundNo, roundMult(roundNo) };
    auto event = EventFactory::create(EventType::Cerberus);
    EventOutcome amb = event->execute(*this, dummy, rcFake, player);
    cout << amb.log << "\n";
    pauseMsg(Config::UI_PAUSE_MS);

    if (player.getFuel() <= 0) {
        cout << "You ran out of fuel. Mission failed.\n";
        return false;
    }
    if (amb.gameOver) return false;
    if (!player.hasCargo()) {
        cout << "Cargo lost. Mission failed.\n";
        return false;
    }
    return true;
}

// Vieno hop'o sprendimas
bool Game::resolveHop(const Route& r, int roundNo, int /*target*/) {
    if (player.getFuel() < r.fuelCost) {
        cout << "Not enough fuel for this jump.\n";
        pauseMsg(Config::UI_PAUSE_MS);
        return true;
    }

    HopContext ctx;
    ctx.edgeFuelBase = r.fuelCost;
    ctx.routeRisk = r.risk;
    ctx.bucket = world.bucket(r.risk);

    double mult = roundMult(roundNo);
    double eventChance = clampd(r.risk * mult, Config::MIN_EVENT_CHANCE, Config::MAX_EVENT_CHANCE);

    cout << "Jumping to " << world.getPlanetName(r.to)
        << " (fuel cost " << r.fuelCost << ", risk " << fixed << setprecision(2) << r.risk
        << ", event chance " << (int)std::round(eventChance * 100) << "%)...\n";

    int fuelAfter = player.getFuel() - r.fuelCost;
    player.setPreviousPlanet(player.getCurrentPlanet());
    player.setCurrentPlanet(r.to);
    player.setFuel(fuelAfter);

    bool occur = (rand01() < eventChance);
    if (!occur) {
        cout << "No incidents. Travel complete.\n";
        pauseMsg(Config::UI_PAUSE_MS);
        return true;
    }

    auto pool = buildEventPool(ctx.bucket, roundNo);

    int totalWeight = 0;
    for (auto& p : pool) totalWeight += p.second;
    if (totalWeight <= 0) {
        cout << "Strange calm... no event triggered.\n";
        pauseMsg(Config::UI_PAUSE_MS);
        return true;
    }

    int roll = randInt(1, totalWeight);
    int acc = 0;
    EventType chosen = EventType::None;
    for (auto& p : pool) {
        acc += p.second;
        if (roll <= acc) { chosen = p.first; break; }
    }
    if (chosen == EventType::None) {
        cout << "Space remains silent.\n";
        pauseMsg(Config::UI_PAUSE_MS);
        return true;
    }

    RoundConfig rc{ roundNo, mult };
    auto ev = EventFactory::create(chosen);
    EventOutcome out = ev->execute(*this, ctx, rc, player);

    cout << out.log << "\n";
    pauseMsg(Config::UI_PAUSE_MS);

    if (player.getFuel() <= 0) {
        cout << "You ran out of fuel. Mission failed.\n";
        return false;
    }
    if (out.gameOver) return false;
    if (!player.hasCargo()) {
        cout << "Cargo lost. Mission failed.\n";
        return false;
    }
    return true;
}

// Vienas round'as
bool Game::runRound(int roundNo, int target) {
    while (true) {
        printStatus(roundNo, target);

        if (player.getCurrentPlanet() == target) {
            cout << "Objective reached: " << world.getPlanetName(target) << ". Round complete!\n";
            int bonus = missionBonus(roundNo);
            player.addCredits(bonus);
            cout << "You received a " << bonus << " credit bonus. Credits: " << player.getCredits() << "\n";
            pauseMsg(Config::UI_PAUSE_MS);
            return true;
        }

        if (!maybeVisitLocalStation(roundNo)) {
            return false;
        }

        if (isStrandedHere()) {
            cout << "You are stranded here with insufficient fuel for any jump. Mission failed.\n";
            return false;
        }

        vector<Route> options;
        showNeighborsWithPreview(roundNo, target, options);

        cout << "Choose a route (number): ";
        int choice;
        if (!(cin >> choice)) return false;

        if (choice == (int)options.size() + 1) {
            cout << "Menu: 1 status  2 quit  3 wait  4 map\n";
            int m; cin >> m;
            if (m == 1) {}
            else if (m == 2) { cout << "Exiting. Thanks for playing.\n"; return false; }
            else if (m == 3) { cout << "You wait and reconsider.\n"; pauseMsg(Config::UI_PAUSE_MS); }
            else if (m == 4) {
                printMap();
                pauseMsg(Config::UI_PAUSE_MS);
            }
            continue;
        }
        if (choice < 1 || choice >(int)options.size()) {
            cout << "Invalid choice.\n"; pauseMsg(Config::UI_PAUSE_MS); continue;
        }
        const Route& rr = options[choice - 1];
        bool ok = resolveHop(rr, roundNo, target);
        if (!ok) return false;
    }
}

// Pagrindinis žaidimo ciklas
void Game::run() {
    initWorld();
    cout << "KOSMINIS KURJERIS\n";
    cout << "Welcome, Commander. Your mission: deliver critical supplies across the galaxy.\n";
    pauseMsg(Config::UI_PAUSE_MS);

    for (int round = 1; round <= rounds; ++round) {
        int target = roundTargets[round - 1];
        cout << "\n=== NEW ROUND " << round << " ===\n";
        bool ok = runRound(round, target);
        if (!ok) {
            cout << "GAME OVER. Final credits: " << player.getCredits() << "\n";
            return;
        }
    }

    cout << "All objectives complete. You successfully delivered the intel to Earth.\n";
    cout << "Final score (credits): " << player.getCredits() << "\n";
}
