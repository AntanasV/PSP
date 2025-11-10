#include <iostream>
#include <vector>
#include <string>
#include <utility>
#include <algorithm>
#include <random>
#include <chrono>
#include <limits>
#include <iomanip>
#include <functional>
#include <cmath>
#include <sstream>
#include <cstdint>
#include <thread>   // delay

using namespace std;

// =================== Config ===================
static const int MSG_DELAY_MS = 1200; // sutrumpintas default delay ~1.2 s

// Maža pagalbinė pauzė
inline void pauseMsg(int ms = MSG_DELAY_MS) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// =================== Utilities ===================
// RNG (global, tik bendram randomui – preview event tipui NAUDOJAM deterministinį rng)
static std::mt19937 rng((uint32_t)chrono::high_resolution_clock::now().time_since_epoch().count());
int randInt(int a, int b) { uniform_int_distribution<int> d(a, b); return d(rng); }
double rand01() { uniform_real_distribution<double> d(0.0, 1.0); return d(rng); }

// Clamp'ai
int clampi(int v, int lo, int hi) { return max(lo, min(hi, v)); }
double clampd(double v, double lo, double hi) { return max(lo, min(hi, v)); }

// Paprastas 64-bit „hash“ sėklai
static uint64_t mix64(uint64_t x) {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

// =================== Domain ===================
enum class RiskBucket { Low, Medium, High };

struct Route {
    int from, to;
    int fuelCost;       // hop'o bazinis kuro kiekis
    double risk;        // 0.10 .. 0.70
    bool bidir = true;
};

struct World {
    vector<string> planets;
    vector<vector<Route>> adj;

    int addPlanet(const string& name) {
        planets.push_back(name);
        adj.push_back({});
        return (int)planets.size() - 1;
    }
    void addRoute(int a, int b, int fuel, double risk, bool bidir = true) {
        Route r1{ a,b,fuel,risk,bidir };
        adj[a].push_back(r1);
        if (bidir) {
            Route r2{ b,a,fuel,risk,bidir };
            adj[b].push_back(r2);
        }
    }
    RiskBucket bucket(double r) const {
        if (r <= 0.30) return RiskBucket::Low;
        if (r <= 0.50) return RiskBucket::Medium;
        return RiskBucket::High;
    }
};

// Atnaujinimai (mod'ai)
struct Upgrades {
    bool weaponsCalibration = false;     // +10% fight
    int  kineticBarriersLevel = 0;       // 0 none, 1:+7% fight & -50% storm fuel loss, 2:+12% fight & -50% storm fuel loss
    bool ediTargeting = false;           // +5% fight, -3% flight fail
    bool engineOverdrive = false;        // -15 pp flight fail; success extra fuel +35% vietoj +50%
    bool cargoDampeners = false;         // -50% cargo loss tikimybių (storm/flight-fail)

    int fightBonus() const {
        int b = 0;
        if (weaponsCalibration) b += 10;
        if (kineticBarriersLevel == 1) b += 7;
        if (kineticBarriersLevel == 2) b += 12;
        if (ediTargeting) b += 5;
        return b;
    }
    int flightFailReductionPP() const {
        int pp = 0;
        if (ediTargeting) pp += 3;
        if (engineOverdrive) pp += 15;
        return pp;
    }
    double stormFuelLossMultiplier() const {
        if (kineticBarriersLevel > 0) return 0.5;
        return 1.0;
    }
    double cargoLossMultiplier() const {
        return cargoDampeners ? 0.5 : 1.0;
    }
};

struct Player {
    int current = -1;
    int prevNode = -1;
    int fuel = 1000;
    int credits = 1000;   // STARTAS: 1000 kredų
    bool cargo = true;    // if lost -> game over
    Upgrades mods;
};

struct RoundConfig {
    int number = 1;              // 1..5
    double difficultyMult = 1.0; // 1:0.8, 2:1.0, 3:1.2, 4:1.4, 5:1.6
};

struct HopContext {
    int edgeFuelBase = 0;
    double routeRisk = 0.0;
    RiskBucket bucket;
};

// =================== Event System ===================
enum class EventType { None, Reaper, Cerberus, Storm, NavigationFail, Station };

struct EventOutcome {
    bool gameOver = false;
    string log;
    bool movedToRandomNeighbor = false;
    bool ambushConvertedToCerberus = false;
};

struct Game; // forward

// Deklaracijos
EventOutcome cerberusEncounter(Game& game, const HopContext& hop, const RoundConfig& rc, Player& pl);
EventOutcome stationEncounter(Game& game, const HopContext& hop, const RoundConfig& rc, Player& pl);

// =================== Game Core ===================
struct Game {
    World world;
    Player player;
    int rounds = 5;
    vector<int> roundTargets; // for 1..5, last is Earth
    int earthId = -1;

    // Raundo sunkumo multiplikatorius (event suveikimo šansui)
    double roundMult(int roundNo) const {
        switch (roundNo) {
        case 1: return 0.8;
        case 2: return 1.0;
        case 3: return 1.2;
        case 4: return 1.4;
        case 5: return 1.6;
        }
        return 1.0;
    }

    // Round completion bonus (didesnis, kad afford'int upgrades)
    int missionBonus(int roundNo) const {
        switch (roundNo) {
        case 1: return 500;
        case 2: return 600;
        case 3: return 700;
        case 4: return 800;
        case 5: return 1000;
        }
        return 500;
    }

    void line() { cout << "------------------------------------------------------------\n"; }

    // Inicijuojame pasaulį ir grafą
    void initWorld() {
        // Planetos (Mass Effect theme)
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

        // Maršrutai (fuel 50..150, risk 0.10..0.70) - STATINIAI
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
        world.addRoute(cit, earth, 100, 0.50);
        world.addRoute(pal, earth, 130, 0.55);

        // Žaidėjo startas
        player.current = eden;
        player.prevNode = -1;
        player.fuel = 1000;
        player.credits = 1000; // kaip susitarėm
        player.cargo = true;

        // Round tikslai: 1..4 random iš sąrašo, 5 - Earth
        vector<int> candidates = { the, nov, pal, ill, omega, fer, hor, tuc, cit };
        shuffle(candidates.begin(), candidates.end(), rng);
        roundTargets.clear();
        for (int i = 0; i < 4 && i < (int)candidates.size(); ++i) roundTargets.push_back(candidates[i]);
        roundTargets.push_back(earth);
    }

    // Statuso spausdinimas
    void printStatus(int roundNo, int target) {
        line();
        cout << "Round " << roundNo << " / " << rounds << "\n";
        cout << "Location: " << world.planets[player.current] << "\n";
        cout << "Objective: " << world.planets[target] << "\n";
        cout << "Fuel: " << player.fuel << "   Credits: " << player.credits << "   Cargo: " << (player.cargo ? "secured" : "LOST") << "\n";
        cout << "Upgrades: ";
        vector<string> m;
        if (player.mods.weaponsCalibration) m.push_back("Weapons Calibration");
        if (player.mods.kineticBarriersLevel == 1) m.push_back("Kinetic Barriers Mk I");
        if (player.mods.kineticBarriersLevel == 2) m.push_back("Kinetic Barriers Mk II");
        if (player.mods.ediTargeting) m.push_back("EDI Targeting");
        if (player.mods.engineOverdrive) m.push_back("Engine Overdrive");
        if (player.mods.cargoDampeners) m.push_back("Cargo Dampeners");
        if (m.empty()) cout << "(none)";
        else {
            for (size_t i = 0; i < m.size(); ++i) { if (i) cout << ", "; cout << m[i]; }
        }
        cout << "\n";
        line();
    }

    // Shop (pigesni atnaujinimai, ASCII minus'ai)
    void openShop(Player& pl) {
        while (true) {
            cout << "Relay Station: you may buy fuel or upgrades. 1 Fuel = 1 Credit.\n";
            cout << "Credits: " << pl.credits << "   Fuel: " << pl.fuel << "\n";
            cout << "1) Buy fuel\n";
            cout << "2) Buy upgrades\n";
            cout << "3) Leave station\n";
            cout << "Select: ";
            int c; if (!(cin >> c)) { return; }
            if (c == 1) {
                cout << "How much fuel to buy? ";
                int amt; cin >> amt;
                if (amt <= 0) { cout << "Nothing purchased.\n"; continue; }
                if (amt > pl.credits) { cout << "Not enough credits.\n"; continue; }
                pl.credits -= amt;
                pl.fuel += amt;
                cout << "Purchased " << amt << " fuel. Now you have " << pl.fuel << " fuel, " << pl.credits << " credits.\n";
            }
            else if (c == 2) {
                while (true) {
                    // Naujos kainos: 600 / 450 / 900 / 400 / 500 / 700
                    cout << "UPGRADES (discounted prices):\n";
                    cout << " 1) Weapons Calibration (+10% fight) - 600\n";
                    cout << " 2) Kinetic Barriers Mk I (+7% fight, -50% storm fuel loss) - 450\n";
                    cout << " 3) Kinetic Barriers Mk II (+12% fight, -50% storm fuel loss) - 900 (replaces Mk I)\n";
                    cout << " 4) EDI Combat Targeting (+5% fight, -3% flight fail) - 400\n";
                    cout << " 5) Engine Overdrive (-15 pp flight fail, success extra fuel +35% instead of +50%) - 500\n";
                    cout << " 6) Cargo Dampeners (-50% cargo loss chances) - 700\n";
                    cout << " 7) Back\n";
                    cout << "Credits: " << pl.credits << "\n";
                    cout << "Select: ";
                    int u; cin >> u;
                    if (u == 7) break;
                    auto buy = [&](int price, function<void()> apply) {
                        if (pl.credits < price) { cout << "Not enough credits.\n"; return; }
                        apply();
                        pl.credits -= price;
                        cout << "Purchased. Remaining credits: " << pl.credits << "\n";
                        };
                    if (u == 1) {
                        if (pl.mods.weaponsCalibration) { cout << "You already own this.\n"; continue; }
                        buy(600, [&] { pl.mods.weaponsCalibration = true; });
                    }
                    else if (u == 2) {
                        if (pl.mods.kineticBarriersLevel >= 1) { cout << "You already have Barriers (Mk " << pl.mods.kineticBarriersLevel << ").\n"; continue; }
                        buy(450, [&] { pl.mods.kineticBarriersLevel = 1; });
                    }
                    else if (u == 3) {
                        if (pl.mods.kineticBarriersLevel == 2) { cout << "You already have Mk II.\n"; continue; }
                        buy(900, [&] { pl.mods.kineticBarriersLevel = 2; });
                    }
                    else if (u == 4) {
                        if (pl.mods.ediTargeting) { cout << "You already own EDI Targeting.\n"; continue; }
                        buy(400, [&] { pl.mods.ediTargeting = true; });
                    }
                    else if (u == 5) {
                        if (pl.mods.engineOverdrive) { cout << "You already own Engine Overdrive.\n"; continue; }
                        buy(500, [&] { pl.mods.engineOverdrive = true; });
                    }
                    else if (u == 6) {
                        if (pl.mods.cargoDampeners) { cout << "You already own Cargo Dampeners.\n"; continue; }
                        buy(700, [&] { pl.mods.cargoDampeners = true; });
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

    // --- Event pool svoriai pagal bucket ir raundą (EARLY buffed stations) ---
    vector<pair<EventType, int>> buildEventPool(RiskBucket b, int roundNo) {
        vector<pair<EventType, int>> pool;
        auto L = [&](int st, int nav, int storm) {
            pool = { {EventType::Station,st},{EventType::NavigationFail,nav},{EventType::Storm,storm} };
            };
        auto M = [&](int cerb, int storm, int nav, int st) {
            pool = { {EventType::Cerberus,cerb},{EventType::Storm,storm},{EventType::NavigationFail,nav},{EventType::Station,st} };
            };
        auto H = [&](int reaper, int cerb, int storm) {
            pool = { {EventType::Reaper,reaper},{EventType::Cerberus,cerb},{EventType::Storm,storm} };
            };

        switch (b) {
        case RiskBucket::Low:
            switch (roundNo) {
            case 1: L(12, 2, 1); break;  // LABAI daug stočių anksti
            case 2: L(9, 3, 2);  break;
            case 3: L(5, 3, 3);  break;
            case 4: L(3, 3, 5);  break;
            default: L(2, 2, 7); break;  // late - audros dominuoja
            }
            break;
        case RiskBucket::Medium:
            switch (roundNo) {
            case 1: M(3, 2, 2, 5); break; // pridėtas papildomas station svoris early
            case 2: M(4, 3, 2, 3); break;
            case 3: M(5, 4, 2, 1); break;
            case 4: M(6, 5, 1, 1); break;
            default: M(7, 6, 1, 0); break;
            }
            break;
        case RiskBucket::High:
            switch (roundNo) {
            case 1: H(1, 4, 3); break;   // reaper reti
            case 2: H(2, 4, 4); break;
            case 3: H(3, 5, 5); break;
            case 4: H(4, 6, 6); break;
            default: H(6, 7, 7); break;  // vėlyvai - žiauriai
            }
            break;
        }
        return pool;
    }

    // --- Deterministinis svorinis pick (kad preview == realybė) ---
    static EventType weightedPickDeterministic(const vector<pair<EventType, int>>& pool, uint64_t seed) {
        uint64_t s = mix64(seed);
        std::mt19937 local((uint32_t)(s ^ (s >> 32)));
        int sum = 0; for (auto& p : pool) sum += p.second;
        uniform_int_distribution<int> d(1, sum);
        int r = d(local), acc = 0;
        for (auto& p : pool) { acc += p.second; if (r <= acc) return p.first; }
        return pool.back().first;
    }

    // --- Gauti deterministinį event tipą konkrečiam (roundNo, from, to) ---
    EventType deterministicEventForRoute(const Route& r, int roundNo) {
        auto pool = buildEventPool(world.bucket(r.risk), roundNo);
        // sėkla: roundNo (8 bit) | from (16 bit) | to (16 bit) | konstanta
        uint64_t seed = (uint64_t)(roundNo & 0xff)
            | ((uint64_t)(r.from & 0xffff) << 8)
            | ((uint64_t)(r.to & 0xffff) << 24)
            | (0x9E3779B97F4A7C15ULL);
        return weightedPickDeterministic(pool, seed);
    }

    // Kaimynų sąrašas su „preview“ (tikimybės ir DETERMINISTINIS event tipas)
    void showNeighborsWithPreview(int roundNo, int /*target*/, vector<Route>& options) {
        cout << "Available jumps from " << world.planets[player.current] << ":\n";
        double mult = roundMult(roundNo);
        for (size_t i = 0; i < options.size(); ++i) {
            const Route& r = options[i];
            RiskBucket b = world.bucket(r.risk);
            string rb = (b == RiskBucket::Low ? "LOW" : (b == RiskBucket::Medium ? "MEDIUM" : "HIGH"));
            int chancePct = (int)std::lround(clampd(r.risk * mult, 0.05, 0.95) * 100.0);
            cout << setw(2) << i + 1 << ") " << world.planets[r.to]
                << "  fuel " << r.fuelCost
                << "  event chance " << rb << " ~" << chancePct << "%";

            EventType hint = deterministicEventForRoute(r, roundNo);
            cout << "  event: ";
            if (hint == EventType::Reaper) cout << "Reaper Assault";
            else if (hint == EventType::Cerberus) cout << "Cerberus Attack";
            else if (hint == EventType::Storm) cout << "Space Storm";
            else if (hint == EventType::NavigationFail) cout << "Navigation Failure";
            else if (hint == EventType::Station) cout << "Fuel/Upgrade Station";
            cout << "\n";
        }
        cout << setw(2) << options.size() + 1 << ") Cancel (menu)\n";
    }

    // Patikrinti „stranded“: turi kuro >0, bet nepakanka nė vienam kaimynui
    bool isStrandedHere() const {
        const auto& opts = world.adj[player.current];
        for (const auto& r : opts) {
            if (player.fuel >= r.fuelCost) return false;
        }
        return true;
    }

    // Vienas hop'as su event sprendimu
    bool resolveHop(const Route& r, int roundNo, int /*target*/) {
        if (player.fuel < r.fuelCost) {
            cout << "Not enough fuel for this jump.\n";
            pauseMsg(800);
            return true; // likti meniu
        }

        HopContext ctx;
        ctx.edgeFuelBase = r.fuelCost;
        ctx.routeRisk = r.risk;
        ctx.bucket = world.bucket(r.risk);

        double mult = roundMult(roundNo);
        double eventChance = clampd(r.risk * mult, 0.05, 0.95);

        player.prevNode = player.current;
        player.current = r.to;
        player.fuel -= r.fuelCost;
        cout << "Jump to " << world.planets[r.to] << "... -" << r.fuelCost << " fuel. Remaining " << player.fuel << ".\n";
        pauseMsg();

        if (player.fuel < 0) { player.fuel = 0; }

        // event tipas deterministinis
        EventType ev = deterministicEventForRoute(r, roundNo);

        // taisyklė: jei tipas Station - event įvyksta GARANTUOTAI,
        // kitaip - pagal eventChance
        bool eventOccurs = (ev == EventType::Station) ? true : (rand01() < eventChance);

        if (!eventOccurs) {
            cout << "Safe arrival. No events happened.\n";
            pauseMsg(800);
            return true;
        }

        EventOutcome out;
        if (ev == EventType::Reaper) {
            out.gameOver = true;
            out.log = "[REAPER ASSAULT] A Reaper strike... no hope remains. Mission failed.";
        }
        else if (ev == EventType::Cerberus) {
            out = cerberusEncounter(*this, ctx, RoundConfig{ roundNo, mult }, player);
        }
        else if (ev == EventType::Storm) {
            if (rand01() < 0.6) {
                int percent = randInt(15, 30);
                double mulStorm = player.mods.stormFuelLossMultiplier();
                int realPct = std::max(1, (int)std::lround(percent * mulStorm));
                int loss = std::max(1, (int)std::lround(player.fuel * (realPct / 100.0)));
                player.fuel = std::max(0, player.fuel - loss);
                out.log = string("[SPACE STORM] Turbulence batters the hull. Fuel loss ")
                    + to_string(realPct) + "% (" + to_string(loss) + "). Remaining "
                    + to_string(player.fuel) + ".";
            }
            else {
                double cargoFailProb = 1.0 * player.mods.cargoLossMultiplier(); // 1.0 or 0.5
                bool loseCargo = (rand01() < cargoFailProb);
                if (loseCargo) {
                    out.gameOver = true;
                    out.log = "[SPACE STORM] Cargo destroyed in the storm. Mission failed.";
                }
                else {
                    out.log = "[SPACE STORM] Cargo narrowly preserved.";
                }
            }
        }
        else if (ev == EventType::NavigationFail) {
            int loss = std::max(80, (int)std::lround(player.fuel * 0.12));
            player.fuel = std::max(0, player.fuel - loss);
            out.log = string("[NAVIGATION FAILURE] Course error. Fuel lost ")
                + to_string(loss) + ". Remaining " + to_string(player.fuel) + ".";
        }
        else if (ev == EventType::Station) {
            out = stationEncounter(*this, ctx, RoundConfig{ roundNo, mult }, player);
        }
        else {
            out.log = "An unexpected anomaly occurred, but no lasting effects.";
        }

        cout << out.log << "\n";
        pauseMsg(800);

        if (player.fuel <= 0) {
            cout << "You ran out of fuel. Mission failed.\n";
            return false;
        }
        if (out.gameOver) return false;
        if (!player.cargo) {
            cout << "Cargo lost. Mission failed.\n";
            return false;
        }
        return true;
    }

    // Vykdome raundą iki tikslo arba pralaimėjimo
    bool runRound(int roundNo, int target) {
        while (true) {
            printStatus(roundNo, target);

            // STRANDED check prieš pasirinkimus
            if (isStrandedHere()) {
                cout << "You are stranded at " << world.planets[player.current] << ". Not enough fuel to reach any route. Mission failed.\n";
                return false;
            }

            if (player.current == target) {
                cout << "Objective reached: " << world.planets[target] << ". Round complete!\n";
                int bonus = missionBonus(roundNo);
                player.credits += bonus;
                cout << "You received a " << bonus << " credit bonus. Credits: " << player.credits << "\n";
                pauseMsg(800);
                return true;
            }

            vector<Route> options = world.adj[player.current];
            if (options.empty()) {
                cout << "Dead end. Nowhere to jump. Mission failed.\n";
                return false;
            }
            shuffle(options.begin(), options.end(), rng);
            showNeighborsWithPreview(roundNo, target, options);

            cout << "Choose route: ";
            int choice; if (!(cin >> choice)) return false;
            if (choice == (int)options.size() + 1) {
                cout << "Menu: 1 status  2 quit  3 wait  4 map\n";
                int m; cin >> m;
                if (m == 1) { /* status already printed */ }
                else if (m == 2) { cout << "Exiting. Thanks for playing.\n"; return false; }
                else if (m == 3) { cout << "You wait and reconsider.\n"; pauseMsg(800); }
                else if (m == 4) {
                    cout << "Map (planets):\n";
                    for (size_t i = 0; i < world.planets.size(); ++i) {
                        cout << setw(2) << i << ": " << world.planets[i] << "\n";
                    }
                    pauseMsg(800);
                }
                continue;
            }
            if (choice<1 || choice>(int)options.size()) {
                cout << "Invalid choice.\n"; pauseMsg(800); continue;
            }
            const Route& r = options[choice - 1];
            bool ok = resolveHop(r, roundNo, target);
            if (!ok) return false;
        }
    }

    // Pagrindinis ciklas
    void run() {
        cout << "Welcome, Commander. 'Space Courier' - Mass Effect themed run.\n";
        pauseMsg(800);
        initWorld();
        for (int round = 1; round <= rounds; ++round) {
            int target = roundTargets[round - 1];
            bool ok = runRound(round, target);
            if (!ok) { cout << "Game over.\n"; return; }
        }
        cout << "FINALE: Earth reached, cargo delivered. Victory!\n";
        cout << "Final tally: Fuel " << player.fuel << ", Credits " << player.credits << ".\n";
    }
};

// =================== Event implementations ===================

// Pagal kovos šansą priskirti reward tier
static int labelVictoryTier(int victoryChancePct) {
    if (victoryChancePct <= 35) return 0; // Low chance -> biggest reward
    if (victoryChancePct <= 55) return 1; // Medium
    return 2;                          // High chance -> smallest reward
}

// Cerberus: fight vs flight
EventOutcome cerberusEncounter(Game& game, const HopContext& hop, const RoundConfig& rc, Player& pl) {
    EventOutcome out;
    cout << "[CERBERUS ATTACK] Hostiles detected. Fight or attempt to flee?\n";
    pauseMsg(800);

    // Bazinis kovos šansas pagal kibirą
    int base = 0;
    if (hop.bucket == RiskBucket::Low) base = 60;
    else if (hop.bucket == RiskBucket::Medium) base = 45;
    else base = 30;

    int roundPenalty = (rc.number - 1) * 5;      // 0,5,10,15,20
    int combatBonus = pl.mods.fightBonus();  // iki +27
    int victoryChance = clampi(base - roundPenalty + combatBonus, 10, 90);

    // Bėgimo nesėkmės šansas
    int flightFail = 15;
    if (hop.bucket == RiskBucket::High) flightFail += 5;
    flightFail += (rc.number - 1) * 2;           // +0,+2,+4,+6,+8
    flightFail = clampi(flightFail - pl.mods.flightFailReductionPP(), 5, 40);

    cout << " Fight victory chance: " << victoryChance << "%";
    int tier = labelVictoryTier(victoryChance);
    if (tier == 0) cout << " (LOW)";
    else if (tier == 1) cout << " (MED)";
    else cout << " (HIGH)";
    cout << "\n";
    int minR = 0, maxR = 0;
    if (tier == 0) { minR = 450; maxR = 650; }   // šiek tiek daugiau salvage
    else if (tier == 1) { minR = 300; maxR = 450; }
    else { minR = 150; maxR = 300; }
    cout << " Possible salvage on victory: " << minR << "-" << maxR << " credits.\n";
    cout << " Flee failure chance: " << flightFail << "%.\n";
    int extraOk = pl.mods.engineOverdrive ? (int)ceil(hop.edgeFuelBase * 0.35) : (int)ceil(hop.edgeFuelBase * 0.50);
    int extraFail = (int)ceil(hop.edgeFuelBase * 1.00);
    cout << " Flee cost: success +" << extraOk << " fuel, failure +" << extraFail
        << " fuel, 25% chance to lose cargo on failure.\n";
    pauseMsg(800);

    cout << "Choose: 1) Fight  2) Flee : ";
    int c; cin >> c;
    if (c == 1) {
        bool win = (rand01() < (victoryChance / 100.0));
        if (win) {
            int reward = randInt(minR, maxR);
            pl.credits += reward;
            out.log = string("[CERBERUS] Victory! Salvage recovered. +")
                + to_string(reward) + " credits. Credits: " + to_string(pl.credits) + ".";
        }
        else {
            bool loseCargo = (rand01() < 0.50);
            if (loseCargo) {
                pl.cargo = false;
                out.gameOver = true;
                out.log = "[CERBERUS] Defeat. Cargo destroyed. Mission failed.";
            }
            else {
                pl.fuel = std::max(0, pl.fuel - 150);
                if (game.player.prevNode != -1) {
                    game.player.current = game.player.prevNode;
                    out.log = string("[CERBERUS] Defeat in battle. -150 fuel. Falling back to ")
                        + game.world.planets[game.player.current] + ".";
                }
                else {
                    out.log = "[CERBERUS] Defeat in battle. -150 fuel. Nowhere to retreat.";
                }
            }
        }
    }
    else {
        bool fail = (rand01() < (flightFail / 100.0));
        if (!fail) {
            int extra = extraOk;
            pl.fuel = std::max(0, pl.fuel - extra);
            out.log = string("[CERBERUS] Successful escape. Extra fuel -") + to_string(extra) + ".";
        }
        else {
            int extra = extraFail;
            pl.fuel = std::max(0, pl.fuel - extra);
            bool loseCargo = (rand01() < (0.25 * pl.mods.cargoLossMultiplier()));
            if (loseCargo) {
                pl.cargo = false;
                out.gameOver = true;
                out.log = string("[CERBERUS] Escape failed. -") + to_string(extra)
                    + " fuel. Cargo lost. Mission failed.";
            }
            else {
                pl.fuel = std::max(0, pl.fuel - 100);
                out.log = string("[CERBERUS] Escape failed. -") + to_string(extra)
                    + " fuel and additional -100 fuel due to damage.";
            }
        }
    }
    return out;
}

// Stotis (shop/wreck/ambush) - kviečiama GARANTUOTAI, jei preview tipas buvo Station
EventOutcome stationEncounter(Game& game, const HopContext& /*hop*/, const RoundConfig& /*rc*/, Player& pl) {
    EventOutcome out;
    cout << "[RELAY STATION] A station is detected in this system. Investigate?\n";
    cout << "1) Yes  2) No\n";
    int c; cin >> c;
    if (c != 1) { out.log = "[STATION] You keep your distance. Journey continues."; return out; }

    int roll = randInt(1, 100);
    if (roll <= 50) {
        out.log = "[STATION] The station is operational. You can trade.";
        cout << out.log << "\n";
        pauseMsg(800);
        game.openShop(pl);
    }
    else if (roll <= 80) {
        int fuelGain = randInt(50, 120);
        int credGain = randInt(100, 240); // kiek daugiau kreditų iš wreck
        pl.fuel += fuelGain;
        pl.credits += credGain;
        out.log = string("[STATION] Wreckage found. +")
            + to_string(fuelGain) + " fuel, +" + to_string(credGain) + " credits.";
    }
    else {
        cout << "[STATION] Ambush! Cerberus forces engage.\n";
        pauseMsg(800);
        // Ambush perėmimas - naudokim tą patį Cerberus encounter
        HopContext dummy{};
        dummy.edgeFuelBase = 0;
        dummy.routeRisk = 0.4;
        dummy.bucket = RiskBucket::Medium;
        RoundConfig rcFake{ 2,1.0 };
        EventOutcome amb = cerberusEncounter(game, dummy, rcFake, pl);
        amb.ambushConvertedToCerberus = true;
        return amb;
    }
    return out;
}

// =================== main ===================
int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    Game g;
    pauseMsg(800);
    g.run();
    return 0;
}
