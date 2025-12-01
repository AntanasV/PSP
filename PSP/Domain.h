#pragma once

#include <string>
#include <vector>
#include "Config.h"

enum class RiskBucket { Low, Medium, High };

struct Route {
    int from;
    int to;
    int fuelCost;    // hop'o bazinis kuro kiekis
    double risk;     // 0.10 .. 0.70
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

    // Nauji upgrade'ai (scanneriai / shield)
    bool hasRouteScanner()  const { return routeScanner; }
    bool hasReaperScanner() const { return reaperScanner; }
    bool hasStormShield()   const { return stormShield; }

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

    void enableRouteScanner() { routeScanner = true; }
    void enableReaperScanner() { reaperScanner = true; }
    void enableStormShield() { stormShield = true; }

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
        double m = 1.0;
        if (kineticBarriersLevel > 0) m *= 0.5;    // barriers
        if (stormShield)              m *= 0.75;   // papildomas -25%
        return m;
    }

    double cargoLossMultiplier() const {
        double m = 1.0;
        if (cargoDampeners) m *= 0.5;
        if (stormShield)    m *= 0.75; // mažina storm cargo loss šansą
        return m;
    }

    // Reaper catch šanso sumažinimas
    int reaperCatchReductionPP() const {
        if (kineticBarriersLevel == 1) return 8;   // Mk I mažina 8 pp
        if (kineticBarriersLevel == 2) return 15;  // Mk II mažina 15 pp
        return 0;
    }

private:
    bool weaponsCalibration = false; // +10% fight
    int  kineticBarriersLevel = 0;   // 0 none, 1:+7% fight, 2:+12% fight
    bool ediTargeting = false;       // +5% fight, -3% flight fail
    bool engineOverdrive = false;    // -15 pp flight fail; success extra fuel +35% vietoj +50%
    bool cargoDampeners = false;     // -50% cargo loss tikimybių (storm/flight-fail)

    bool routeScanner = false;      // rodo preview
    bool reaperScanner = false;      // (jei norėtum rodyti catch chance)
    bool stormShield = false;      // mažina storm nuostolius
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
    void setFuel(int f) { fuel = (f < 0 ? 0 : f); }
    void addFuel(int delta) {
        fuel += delta;
        if (fuel < 0) fuel = 0;
    }
    bool spendFuel(int amount) {
        if (amount < 0) return false;
        if (fuel < amount) return false;
        fuel -= amount;
        return true;
    }

    // Credits
    int  getCredits() const { return credits; }
    void setCredits(int c) { credits = (c < 0 ? 0 : c); }
    void addCredits(int delta) {
        credits += delta;
        if (credits < 0) credits = 0;
    }
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
    int fuel = Config::STARTING_FUEL;
    int credits = Config::STARTING_CREDITS;
    bool cargo = true;    // if lost -> game over
    Upgrades mods;
};

// Pasaulis - enkapsuliuotas
class World {
public:
    int addPlanet(const std::string& name);
    void addRoute(int a, int b, int fuel, double risk, bool bidir = true);

    RiskBucket bucket(double r) const;

    const std::string& getPlanetName(int id) const;
    int getPlanetCount() const;
    const std::vector<Route>& getRoutesFrom(int planetId) const;

private:
    std::vector<std::string> planets;
    std::vector<std::vector<Route>> adj;
};

struct RoundConfig {
    int number = 1;              // 1..5
    double difficultyMult = 1.0; // 1:0.8, 2:1.0, 3:1.2, 4:1.4, 5:1.6
};

struct HopContext {
    int edgeFuelBase = 0;
    double routeRisk = 0.0;
    RiskBucket bucket = RiskBucket::Low; // svarbu inicijuoti
};
