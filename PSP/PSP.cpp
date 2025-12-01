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
#include <thread>

using namespace std;

// =================== Config ===================
static const int MSG_DELAY_MS = 1200; //delay ~1.2 s

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

// Atnaujinimai (mod'ai) - enkapsuliuoti
class Upgrades {
public:
    // Getteriai
    bool hasWeaponsCalibration() const { return weaponsCalibration; }
    int  getKineticBarriersLevel() const { return kineticBarriersLevel; } // 0,1,2
    bool hasEdiTargeting() const { return ediTargeting; }
    bool hasEngineOverdrive() const { return engineOverdrive; }
    bool hasCargoDampeners() const { return cargoDampeners; }

    // "Setteriai" / veiksmų metodai
    void buyWeaponsCalibration() { weaponsCalibration = true; }
    void setKineticBarriersLevel(int level) {
        if (level < 0) level = 0;
        if (level > 2) level = 2;
        kineticBarriersLevel = level;
    }
    void enableEdiTargeting() { ediTargeting = true; }
    void enableEngineOverdrive() { engineOverdrive = true; }
    void enableCargoDampeners() { cargoDampeners = true; }

    // Bonus skaičiavimai
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

    // Reaper catch šanso sumažinimas
    int reaperCatchReductionPP() const {
        if (kineticBarriersLevel == 1) return 8;   // Mk I mažina 8 pp
        if (kineticBarriersLevel == 2) return 15;  // Mk II mažina 15 pp
        return 0;
    }

private:
    bool weaponsCalibration = false; // +10% fight
    int  kineticBarriersLevel = 0;   // 0 none, 1:+7% fight & -50% storm fuel loss, 2:+12% fight & -50% storm fuel loss
    bool ediTargeting = false;       // +5% fight, -3% flight fail
    bool engineOverdrive = false;    // -15 pp flight fail; success extra fuel +35% vietoj +50%
    bool cargoDampeners = false;     // -50% cargo loss tikimybių (storm/flight-fail)
};

// Žaidėjas - enkapsuliuotas
class Player {
public:
    Player() = default;

    // Planetos
    int  getCurrentPlanet() const { return current; }
    void setCurrentPlanet(int id) { current = id; }

    int  getPreviousPlanet() const { return prevNode; }
    void setPreviousPlanet(int id) { prevNode = id; }

    // Fuel
    int  getFuel() const { return fuel; }
    void setFuel(int f) { fuel = max(0, f); }
    void addFuel(int delta) { fuel = max(0, fuel + delta); }
    bool spendFuel(int amount) {
        if (amount < 0) return false;
        if (fuel < amount) return false;
        fuel -= amount;
        return true;
    }

    // Credits
    int  getCredits() const { return credits; }
    void setCredits(int c) { credits = max(0, c); }
    void addCredits(int delta) { credits = max(0, credits + delta); }
    bool spendCredits(int amount) {
        if (amount < 0) return false;
        if (credits < amount) return false;
        credits -= amount;
        return true;
    }

    // Cargo
    bool hasCargo() const { return cargo; }
    void loseCargo() { cargo = false; }
    void resetCargo() { cargo = true; }

    // Upgrades
    Upgrades& getUpgrades() { return mods; }
    const Upgrades& getUpgrades() const { return mods; }

private:
    int current = -1;
    int prevNode = -1;
    int fuel = 1000;
    int credits = 1000;   // STARTAS: 1000 kredų
    bool cargo = true;    // if lost -> game over
    Upgrades mods;
};

// Pasaulis - enkapsuliuotas
class World {
public:
    int addPlanet(const string& name) {
        planets.push_back(name);
        adj.push_back({});
        return static_cast<int>(planets.size()) - 1;
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

    const string& getPlanetName(int id) const {
        return planets.at(id);
    }

    int getPlanetCount() const {
        return static_cast<int>(planets.size());
    }

    const vector<Route>& getRoutesFrom(int planetId) const {
        return adj.at(planetId);
    }

private:
    vector<string> planets;
    vector<vector<Route>> adj;
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
enum class EventType { None, Reaper, Cerberus, Storm, NavigationFail };

struct EventOutcome {
    bool gameOver = false;
    string log;
};

struct Game; // forward

// Deklaracijos
EventOutcome cerberusEncounter(Game& game, const HopContext& hop, const RoundConfig& rc, Player& pl);
EventOutcome reaperEncounter(Game& game, const HopContext& hop, const RoundConfig& rc, Player& pl);

// =================== Game Core ===================
struct Game {
    World world;
    Player player;
    int rounds = 5;
    vector<int> roundTargets; // for 1..5, last is Earth
    int earthId = -1;

    // Planetų station naudotas ar ne
    vector<bool> stationVisited; // true: ši planeta jau tikrinta (operational/wreck/ambush)

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
        player.setCurrentPlanet(eden);
        player.setPreviousPlanet(-1);
        player.setFuel(1000);
        player.setCredits(1000);
        player.resetCargo();

        // Round tikslai: 1..4 random iš sąrašo, 5 - Earth
        vector<int> candidates = { the, nov, pal, ill, omega, fer, hor, tuc, cit };
        shuffle(candidates.begin(), candidates.end(), rng);
        roundTargets.clear();
        for (int i = 0; i < 4 && i < (int)candidates.size(); ++i) roundTargets.push_back(candidates[i]);
        roundTargets.push_back(earth);

        // station visited flags
        stationVisited.assign(world.getPlanetCount(), false);
    }

    // Statuso spausdinimas
    void printStatus(int roundNo, int target) {
        line();
        cout << "Round " << roundNo << " / " << rounds << "\n";
        cout << "Location: " << world.getPlanetName(player.getCurrentPlanet()) << "\n";
        cout << "Objective: " << world.getPlanetName(target) << "\n";
        cout << "Fuel: " << player.getFuel()
            << "   Credits: " << player.getCredits()
            << "   Cargo: " << (player.hasCargo() ? "secured" : "LOST") << "\n";
        cout << "Upgrades: ";
        vector<string> m;
        const Upgrades& mods = player.getUpgrades();
        if (mods.hasWeaponsCalibration()) m.push_back("Weapons Calibration");
        if (mods.getKineticBarriersLevel() == 1) m.push_back("Kinetic Barriers Mk I");
        if (mods.getKineticBarriersLevel() == 2) m.push_back("Kinetic Barriers Mk II");
        if (mods.hasEdiTargeting()) m.push_back("EDI Targeting");
        if (mods.hasEngineOverdrive()) m.push_back("Engine Overdrive");
        if (mods.hasCargoDampeners()) m.push_back("Cargo Dampeners");
        if (m.empty()) cout << "(none)";
        else {
            for (size_t i = 0; i < m.size(); ++i) { if (i) cout << ", "; cout << m[i]; }
        }
        cout << "\n";
        line();
    }

    // ASCII žemėlapis
    void printMap() {
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

    // Shop
    void openShop(Player& pl) {
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
                    // Naujos kainos: 600 / 450 / 900 / 400 / 500 / 700
                    cout << "UPGRADES (discounted prices):\n";
                    cout << " 1) Weapons Calibration (+10% fight) - 600\n";
                    cout << " 2) Kinetic Barriers Mk I (+7% fight, -50% storm fuel loss, -Reaper catch) - 450\n";
                    cout << " 3) Kinetic Barriers Mk II (+12% fight, -50% storm fuel loss, more -Reaper catch) - 900 (replaces Mk I)\n";
                    cout << " 4) EDI Combat Targeting (+5% fight, -3% flight fail) - 400\n";
                    cout << " 5) Engine Overdrive (-15 pp flight fail, success extra fuel +35% instead of +50%) - 500\n";
                    cout << " 6) Cargo Dampeners (-50% cargo loss chances) - 700\n";
                    cout << " 7) Back\n";
                    cout << "Credits: " << pl.getCredits() << "\n";
                    cout << "Select: ";
                    int u; cin >> u;
                    if (u == 7) break;

                    auto buy = [&](int price, function<void(Upgrades&)> apply) {
                        if (pl.getCredits() < price) {
                            cout << "Not enough credits.\n";
                            return;
                        }
                        apply(pl.getUpgrades());
                        pl.spendCredits(price);
                        cout << "Purchased. Remaining credits: " << pl.getCredits() << "\n";
                        };

                    Upgrades& mods = pl.getUpgrades();

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
                        if (mods.hasCargoDampeners()) { cout << "You already own Cargo Dampeners.\n"; continue; }
                        buy(700, [](Upgrades& m) { m.enableCargoDampeners(); });
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

    // --- Event pool svoriai pagal bucket ir raundą ---
    // Reaper'ai matomi High risk bucket, bet jau nuo early game (mažas weight)
    vector<pair<EventType, int>> buildEventPool(RiskBucket b, int roundNo) {
        vector<pair<EventType, int>> pool;
        auto L = [&](int nav, int storm) {
            pool = { {EventType::NavigationFail,nav},{EventType::Storm,storm} };
            };
        auto M = [&](int cerb, int storm, int nav) {
            pool = { {EventType::Cerberus,cerb},{EventType::Storm,storm},{EventType::NavigationFail,nav} };
            };
        auto H = [&](int reaper, int cerb, int storm) {
            pool = { {EventType::Reaper,reaper},{EventType::Cerberus,cerb},{EventType::Storm,storm} };
            };

        switch (b) {
        case RiskBucket::Low:
            switch (roundNo) {
            case 1: L(3, 2); break;
            case 2: L(4, 3); break;
            case 3: L(4, 4); break;
            case 4: L(3, 5); break;
            default: L(2, 7); break;
            }
            break;
        case RiskBucket::Medium:
            switch (roundNo) {
            case 1: M(3, 2, 2); break;
            case 2: M(4, 3, 2); break;
            case 3: M(5, 4, 2); break;
            case 4: M(6, 5, 1); break;
            default: M(7, 6, 1); break;
            }
            break;
        case RiskBucket::High:
            // Early: reaper weight labai žemas, vėliau kyla, bet ne ekstremaliai
            switch (roundNo) {
            case 1: H(1, 4, 4); break;   // 1/9 ~11% Reaper event, maža tikimybė + maži catch šansai
            case 2: H(2, 4, 5); break;
            case 3: H(3, 5, 5); break;
            case 4: H(4, 6, 6); break;
            default: H(5, 7, 7); break;  // vėlyvai - daugiau Reaper presence, bet catch <=50%
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
        cout << "Available jumps from " << world.getPlanetName(player.getCurrentPlanet()) << ":\n";
        double mult = roundMult(roundNo);
        for (size_t i = 0; i < options.size(); ++i) {
            const Route& r = options[i];
            RiskBucket b = world.bucket(r.risk);
            string rb = (b == RiskBucket::Low ? "LOW" : (b == RiskBucket::Medium ? "MEDIUM" : "HIGH"));
            int chancePct = (int)std::lround(clampd(r.risk * mult, 0.05, 0.95) * 100.0);
            cout << setw(2) << i + 1 << ") " << world.getPlanetName(r.to)
                << "  fuel " << r.fuelCost
                << "  event chance " << rb << " ~" << chancePct << "%";

            EventType hint = deterministicEventForRoute(r, roundNo);
            cout << "  event: ";
            if (hint == EventType::Reaper) cout << "Reaper Presence";
            else if (hint == EventType::Cerberus) cout << "Cerberus Attack";
            else if (hint == EventType::Storm) cout << "Space Storm";
            else if (hint == EventType::NavigationFail) cout << "Navigation Failure";
            else cout << "None";
            cout << "\n";
        }
        cout << setw(2) << options.size() + 1 << ") Cancel (menu)\n";
    }

    // Patikrinti „stranded“: turi kuro >0, bet nepakanka nė vienam kaimynui
    bool isStrandedHere() const {
        const auto& opts = world.getRoutesFrom(player.getCurrentPlanet());
        for (const auto& r : opts) {
            if (player.getFuel() >= r.fuelCost) return false;
        }
        return true;
    }

    // Pasiūlyti aplankyti vietinę stotį šioje planetoje (1 kartą per planetą)
    bool maybeVisitLocalStation(int roundNo) {
        int p = player.getCurrentPlanet();
        if (p < 0 || p >= (int)stationVisited.size()) return true;
        if (stationVisited[p]) return true; // jau tikrinta

        cout << "[RELAY STATION] Local fuel/upgrade depot on " << world.getPlanetName(p) << ". Visit?\n";
        cout << "1) Yes  2) No\n";
        int c; if (!(cin >> c)) return false;
        if (c != 1) {
            cout << "You ignore the local station for now.\n";
            pauseMsg(800);
            return true;
        }

        stationVisited[p] = true; // šita planeta jau patikrinta

        int roll = randInt(1, 100);
        if (roll <= 50) {
            cout << "[STATION] The depot is operational. You can trade.\n";
            pauseMsg(800);
            openShop(player);
            return true;
        }
        else if (roll <= 80) {
            int fuelGain = randInt(50, 120);
            int credGain = randInt(100, 240);
            player.addFuel(fuelGain);
            player.addCredits(credGain);
            cout << "[STATION] You find wreckage of an old depot. +"
                << fuelGain << " fuel, +" << credGain << " credits.\n";
            pauseMsg(800);
            return true;
        }
        else {
            cout << "[STATION] Ambush! Cerberus forces engage as you approach the depot.\n";
            pauseMsg(800);
            HopContext dummy{};
            dummy.edgeFuelBase = 0;
            dummy.routeRisk = 0.4;
            dummy.bucket = RiskBucket::Medium;
            RoundConfig rcFake{ roundNo, roundMult(roundNo) };
            EventOutcome amb = cerberusEncounter(*this, dummy, rcFake, player);
            cout << amb.log << "\n";
            pauseMsg(800);

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
    }

    // Vienas hop'as su event sprendimu
    bool resolveHop(const Route& r, int roundNo, int /*target*/) {
        if (player.getFuel() < r.fuelCost) {
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

        player.setPreviousPlanet(player.getCurrentPlanet());
        player.setCurrentPlanet(r.to);
        player.spendFuel(r.fuelCost);
        cout << "Jump to " << world.getPlanetName(r.to)
            << "... -" << r.fuelCost << " fuel. Remaining " << player.getFuel() << ".\n";
        pauseMsg();

        if (player.getFuel() < 0) { player.setFuel(0); }

        // event tipas deterministinis
        EventType ev = deterministicEventForRoute(r, roundNo);

        bool eventOccurs = (rand01() < eventChance);

        if (!eventOccurs || ev == EventType::None) {
            cout << "Safe arrival. No events happened.\n";
            pauseMsg(800);
            return true;
        }

        EventOutcome out;
        if (ev == EventType::Reaper) {
            out = reaperEncounter(*this, ctx, RoundConfig{ roundNo, mult }, player);
        }
        else if (ev == EventType::Cerberus) {
            out = cerberusEncounter(*this, ctx, RoundConfig{ roundNo, mult }, player);
        }
        else if (ev == EventType::Storm) {
            if (rand01() < 0.6) {
                int percent = randInt(15, 30);
                double mulStorm = player.getUpgrades().stormFuelLossMultiplier();
                int realPct = std::max(1, (int)std::lround(percent * mulStorm));
                int loss = std::max(1, (int)std::lround(player.getFuel() * (realPct / 100.0)));
                player.setFuel(std::max(0, player.getFuel() - loss));
                out.log = string("[SPACE STORM] Turbulence batters the hull. Fuel loss ")
                    + to_string(realPct) + "% (" + to_string(loss) + "). Remaining "
                    + to_string(player.getFuel()) + ".";
            }
            else {
                double cargoFailProb = 1.0 * player.getUpgrades().cargoLossMultiplier(); // 1.0 or 0.5
                bool loseCargo = (rand01() < cargoFailProb);
                if (loseCargo) {
                    player.loseCargo();
                    out.gameOver = true;
                    out.log = "[SPACE STORM] Cargo destroyed in the storm. Mission failed.";
                }
                else {
                    out.log = "[SPACE STORM] Cargo narrowly preserved.";
                }
            }
        }
        else if (ev == EventType::NavigationFail) {
            int loss = std::max(80, (int)std::lround(player.getFuel() * 0.12));
            player.setFuel(std::max(0, player.getFuel() - loss));
            out.log = string("[NAVIGATION FAILURE] Course error. Fuel lost ")
                + to_string(loss) + ". Remaining " + to_string(player.getFuel()) + ".";
        }
        else {
            out.log = "An unexpected anomaly occurred, but no lasting effects.";
        }

        cout << out.log << "\n";
        pauseMsg(800);

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

    // Vykdome raundą iki tikslo arba pralaimėjimo
    bool runRound(int roundNo, int target) {
        while (true) {
            printStatus(roundNo, target);

            // Jei jau pasiektas tikslas – pirma užfiksuojam pergalę, be station
            if (player.getCurrentPlanet() == target) {
                cout << "Objective reached: " << world.getPlanetName(target) << ". Round complete!\n";
                int bonus = missionBonus(roundNo);
                player.addCredits(bonus);
                cout << "You received a " << bonus << " credit bonus. Credits: " << player.getCredits() << "\n";
                pauseMsg(800);
                return true;
            }

            // Suteikiam šansą aplankyti lokalų depot (1 kartas šioje planetoje)
            if (!maybeVisitLocalStation(roundNo)) {
                // žaidėjas galėjo mirti ambush'e
                return false;
            }

            // STRANDED check po station (gal depot išgelbėjo)
            if (isStrandedHere()) {
                cout << "You are stranded at " << world.getPlanetName(player.getCurrentPlanet())
                    << ". Not enough fuel to reach any route. Mission failed.\n";
                return false;
            }

            const auto& neigh = world.getRoutesFrom(player.getCurrentPlanet());
            vector<Route> options = neigh;
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
                    printMap();
                    pauseMsg(800);
                }
                continue;
            }
            if (choice < 1 || choice >(int)options.size()) {
                cout << "Invalid choice.\n"; pauseMsg(800); continue;
            }
            const Route& r = options[choice - 1];
            bool ok = resolveHop(r, roundNo, target);
            if (!ok) return false;
        }
    }

    // Pagrindinis ciklas
    void run() {
        cout << "Welcome, Commander. 'Kosminis Kurjeris'.\n";
        pauseMsg(800);
        initWorld();
        for (int round = 1; round <= rounds; ++round) {
            int target = roundTargets[round - 1];
            bool ok = runRound(round, target);
            if (!ok) { cout << "Game over.\n"; return; }
        }
        cout << "FINALE: Earth reached, cargo delivered. Victory!\n";
        cout << "Final tally: Fuel " << player.getFuel()
            << ", Credits " << player.getCredits() << ".\n";
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

    const Upgrades& mods = pl.getUpgrades();

    // Bazinis kovos šansas pagal kibirą
    int base = 0;
    if (hop.bucket == RiskBucket::Low) base = 60;
    else if (hop.bucket == RiskBucket::Medium) base = 45;
    else base = 30;

    int roundPenalty = (rc.number - 1) * 5;      // 0,5,10,15,20
    int combatBonus = mods.fightBonus();         // iki +27
    int victoryChance = clampi(base - roundPenalty + combatBonus, 10, 90);

    // Bėgimo nesėkmės šansas
    int flightFail = 15;
    if (hop.bucket == RiskBucket::High) flightFail += 5;
    flightFail += (rc.number - 1) * 2;           // +0,+2,+4,+6,+8
    flightFail = clampi(flightFail - mods.flightFailReductionPP(), 5, 40);

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
    int extraOk = mods.hasEngineOverdrive() ? (int)ceil(hop.edgeFuelBase * 0.35) : (int)ceil(hop.edgeFuelBase * 0.50);
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
            pl.addCredits(reward);
            out.log = string("[CERBERUS] Victory! Salvage recovered. +")
                + to_string(reward) + " credits. Credits: " + to_string(pl.getCredits()) + ".";
        }
        else {
            bool loseCargo = (rand01() < 0.50);
            if (loseCargo) {
                pl.loseCargo();
                out.gameOver = true;
                out.log = "[CERBERUS] Defeat. Cargo destroyed. Mission failed.";
            }
            else {
                int newFuel = max(0, pl.getFuel() - 150);
                pl.setFuel(newFuel);
                if (game.player.getPreviousPlanet() != -1) {
                    game.player.setCurrentPlanet(game.player.getPreviousPlanet());
                    out.log = string("[CERBERUS] Defeat in battle. -150 fuel. Falling back to ")
                        + game.world.getPlanetName(game.player.getCurrentPlanet()) + ".";
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
            pl.setFuel(max(0, pl.getFuel() - extra));
            out.log = string("[CERBERUS] Successful escape. Extra fuel -") + to_string(extra) + ".";
        }
        else {
            int extra = extraFail;
            pl.setFuel(max(0, pl.getFuel() - extra));
            bool loseCargo = (rand01() < (0.25 * mods.cargoLossMultiplier()));
            if (loseCargo) {
                pl.loseCargo();
                out.gameOver = true;
                out.log = string("[CERBERUS] Escape failed. -") + to_string(extra)
                    + " fuel. Cargo lost. Mission failed.";
            }
            else {
                pl.setFuel(max(0, pl.getFuel() - 100));
                out.log = string("[CERBERUS] Escape failed. -") + to_string(extra)
                    + " fuel and additional -100 fuel due to damage.";
            }
        }
    }
    return out;
}

// Reaper encounter: šansas būti pagautam (early mažas, late didesnis, max 50%),
// Kinetic Barriers sumažina šitą catch šansą.
EventOutcome reaperEncounter(Game& /*game*/, const HopContext& hop, const RoundConfig& rc, Player& pl) {
    EventOutcome out;
    cout << "[REAPER SIGNATURE] A Reaper signal flickers at the edge of your sensors...\n";
    pauseMsg(800);

    const Upgrades& mods = pl.getUpgrades();

    int baseCatch = 0;
    // Bazė pagal risk bucket + raundą
    if (hop.bucket == RiskBucket::High) {
        // 10, 20, 30, 40, 50
        baseCatch = 10 + (rc.number - 1) * 10;
    }
    else if (hop.bucket == RiskBucket::Medium) {
        // 5, 10, 15, 20, 25
        baseCatch = 5 + (rc.number - 1) * 5;
    }
    else {
        // Low: labai maži šansai
        // 3, 5, 7, 9, 11 (realiai ~3-11%)
        baseCatch = 3 + (rc.number - 1) * 2;
    }

    // Kinetic Barriers mažina catch procentus
    baseCatch = clampi(baseCatch - mods.reaperCatchReductionPP(), 0, 50);

    cout << " Reaper lock-on chance: " << baseCatch << "%";
    if (mods.getKineticBarriersLevel() > 0) {
        cout << " (reduced by Kinetic Barriers Mk " << mods.getKineticBarriersLevel() << ")";
    }
    cout << "\n";
    cout << " If caught: total annihilation.\n";
    cout << " If you evade: evasive maneuvers cost some fuel.\n";
    pauseMsg(800);

    bool caught = (rand01() < (baseCatch / 100.0));
    if (caught) {
        out.gameOver = true;
        out.log = "[REAPER] The Reaper acquires a solid lock. There is no escape. Mission failed.";
    }
    else {
        int baseFuel = (hop.edgeFuelBase > 0 ? hop.edgeFuelBase : 80);
        int loss = std::max(30, (int)std::lround(baseFuel * 0.5)); // 50% hop cost, min 30
        pl.setFuel(max(0, pl.getFuel() - loss));
        out.log = string("[REAPER] You slip past the Reaper's gaze, but hard burns cost ")
            + to_string(loss) + " fuel. Remaining " + to_string(pl.getFuel()) + ".";
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
