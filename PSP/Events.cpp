#include "Events.h"
#include "Config.h"
#include "Utils.h"
#include "Game.h"

#include <iostream>
#include <cmath>
#include <algorithm>
#include <functional>

using namespace std;

// Pagal kovos šansą priskirti reward tier
static int labelVictoryTier(int victoryChancePct) {
    if (victoryChancePct <= Config::CERB_TIER1_MAX_VICTORY_CHANCE) return 0; // Low chance -> biggest reward
    if (victoryChancePct <= Config::CERB_TIER2_MAX_VICTORY_CHANCE) return 1; // Medium
    return 2;                             // High chance -> smallest reward
}

// EventFactory
std::unique_ptr<GameEvent> EventFactory::create(EventType type) {
    switch (type) {
    case EventType::Cerberus:       return std::make_unique<CerberusEvent>();
    case EventType::Reaper:         return std::make_unique<ReaperEvent>();
    case EventType::Storm:          return std::make_unique<StormEvent>();
    case EventType::NavigationFail: return std::make_unique<NavigationFailEvent>();
    case EventType::None:
    default:
        return nullptr;
    }
}

// Template Method realizacija
EventOutcome GameEvent::execute(Game& game, const HopContext& hop,
    const RoundConfig& rc, Player& pl) {
    return doExecute(game, hop, rc, pl);
}

// Cerberus event
EventOutcome CerberusEvent::doExecute(Game& game, const HopContext& hop,
    const RoundConfig& rc, Player& pl) {
    EventOutcome out;

    const Upgrades& mods = pl.getUpgrades();

    int base = 0;
    switch (hop.bucket) {
    case RiskBucket::Low:    base = 50; break;
    case RiskBucket::Medium: base = 60; break;
    case RiskBucket::High:   base = 70; break;
    }

    // Weapon + barriers + EDI
    int combatBonus = mods.fightBonus();

    // Round penalty (vėlesniuose raunduose sunkiau)
    int roundPenalty = (rc.number - 1) * 4;

    int victoryChance = clampi(base - roundPenalty + combatBonus, 10, 90);

    cout << "[CERBERUS] Hostile ships detected along the route!\n";
    cout << " Base combat roll: " << base << "%  | Round penalty: -" << roundPenalty
        << "%  | Upgrades bonus: +" << combatBonus << "%\n";

    cout << " Fight victory chance: " << victoryChance << "%";
    int tier = labelVictoryTier(victoryChance);
    if (tier == 0) cout << " (LOW)";
    else if (tier == 1) cout << " (MED)";
    else cout << " (HIGH)";
    cout << "\n";

    int minR = 0, maxR = 0;
    if (tier == 0) {
        minR = Config::CERB_REWARD_LOW_MIN;
        maxR = Config::CERB_REWARD_LOW_MAX;
    }
    else if (tier == 1) {
        minR = Config::CERB_REWARD_MED_MIN;
        maxR = Config::CERB_REWARD_MED_MAX;
    }
    else {
        minR = Config::CERB_REWARD_HIGH_MIN;
        maxR = Config::CERB_REWARD_HIGH_MAX;
    }

    cout << " Possible salvage on victory: " << minR << "-" << maxR << " credits.\n";

    // FLEE FAIL ŠANSAS (flightFail) – čia buvo nedefinuotas, dabar apskaičiuojam
    int flightFail = 25; // bazinis
    if (hop.bucket == RiskBucket::Medium) flightFail += 10;
    if (hop.bucket == RiskBucket::High)   flightFail += 20;

    // round sunkumas
    flightFail += (rc.number - 1) * 3;

    // upgradų įtaka (EDI + EngineOverdrive)
    flightFail -= mods.flightFailReductionPP();

    // apribojam ribas
    flightFail = clampi(flightFail, 5, 90);

    cout << " Flee failure chance: " << flightFail << "%.\n";

    int extraOk = mods.hasEngineOverdrive()
        ? (int)ceil(hop.edgeFuelBase * Config::CERB_ENGINE_OVERDRIVE_EXTRA_OK_MULT)
        : (int)ceil(hop.edgeFuelBase * Config::CERB_ENGINE_DEFAULT_EXTRA_OK_MULT);
    int extraFail = (int)ceil(hop.edgeFuelBase * Config::CERB_EXTRA_FAIL_MULT);

    cout << " Flee cost: success +" << extraOk << " fuel, failure +" << extraFail
        << " fuel, 25% chance to lose cargo on failure.\n";
    pauseMsg(Config::UI_PAUSE_MS);

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
            bool loseCargo = (rand01() < (2 * Config::CERB_BASE_CARGO_LOSS_PROB));
            if (loseCargo) {
                pl.loseCargo();
                out.gameOver = true;
                out.log = "[CERBERUS] Defeat. Cargo destroyed. Mission failed.";
            }
            else {
                int newFuel = max(0, pl.getFuel() - Config::CERB_DEFEAT_FUEL_LOSS);
                pl.setFuel(newFuel);
                if (game.player.getPreviousPlanet() != -1) {
                    game.player.setCurrentPlanet(game.player.getPreviousPlanet());
                    out.log = string("[CERBERUS] Defeat in battle. -")
                        + to_string(Config::CERB_DEFEAT_FUEL_LOSS)
                        + " fuel. Falling back to "
                        + "previous location.";
                }
                else {
                    out.log = string("[CERBERUS] Defeat in battle. -")
                        + to_string(Config::CERB_DEFEAT_FUEL_LOSS)
                        + " fuel. Nowhere to retreat.";
                }
            }
        }
    }
    else {
        // BANDYMAS SPRUKTI
        bool fail = (rand01() < (flightFail / 100.0));
        int extra = fail ? extraFail : extraOk;
        pl.setFuel(max(0, pl.getFuel() - extra));
        if (fail) {
            bool loseCargo = (rand01() < (Config::CERB_BASE_CARGO_LOSS_PROB * mods.cargoLossMultiplier()));
            if (loseCargo) {
                pl.loseCargo();
                out.gameOver = true;
                out.log = string("[CERBERUS] Flee failed. -") + to_string(extra)
                    + " fuel. Cargo destroyed. Mission failed.";
            }
            else {
                out.log = string("[CERBERUS] Flee failed. -") + to_string(extra)
                    + " fuel, but cargo survived.";
            }
        }
        else {
            out.log = string("[CERBERUS] Flee successful. -") + to_string(extra)
                + " fuel.";
        }
    }

    return out;
}

// Reaper event
EventOutcome ReaperEvent::doExecute(Game& /*game*/, const HopContext& hop,
    const RoundConfig& rc, Player& pl) {
    EventOutcome out;

    int baseCatch = 0;
    switch (hop.bucket) {
    case RiskBucket::Low:    baseCatch = Config::REAPER_LOW_START; break;
    case RiskBucket::Medium: baseCatch = Config::REAPER_MED_START; break;
    case RiskBucket::High:   baseCatch = Config::REAPER_HIGH_START; break;
    }

    baseCatch += (rc.number - 1) * 5;

    const Upgrades& mods = pl.getUpgrades();
    baseCatch -= mods.reaperCatchReductionPP();
    baseCatch = clampi(baseCatch, 5, Config::REAPER_MAX_CATCH);

    cout << "[REAPER] A Reaper's gaze brushes past your ship...\n";
    cout << " Base detection chance: " << baseCatch << "%\n";
    pauseMsg(Config::UI_PAUSE_MS);

    bool caught = (rand01() < (baseCatch / 100.0));
    if (!caught) {
        out.log = "[REAPER] You slip past unnoticed... this time.";
        return out;
    }

    int loss = std::max(
        Config::REAPER_EVASIVE_MIN_LOSS,
        (int)std::round(Config::REAPER_EVASIVE_BASE_FUEL +
            hop.edgeFuelBase * Config::REAPER_EVASIVE_FUEL_MULT)
    );
    pl.setFuel(max(0, pl.getFuel() - loss));
    out.log = string("[REAPER] You slip past the Reaper's gaze, but hard burns cost ")
        + to_string(loss) + " fuel. Remaining " + to_string(pl.getFuel()) + ".";
    return out;
}

// Space Storm event
EventOutcome StormEvent::doExecute(Game& /*game*/, const HopContext& /*hop*/,
    const RoundConfig& /*rc*/, Player& pl) {
    EventOutcome out;
    if (rand01() < Config::STORM_EVENT_PROBABILITY) {
        int percent = randInt(Config::STORM_FUEL_LOSS_MIN_PCT, Config::STORM_FUEL_LOSS_MAX_PCT);
        double mulStorm = pl.getUpgrades().stormFuelLossMultiplier();
        int realPct = max(1, (int)std::lround(percent * mulStorm));
        int loss = max(1, (int)std::lround(pl.getFuel() * (realPct / 100.0)));
        pl.setFuel(max(0, pl.getFuel() - loss));
        out.log = string("[SPACE STORM] Turbulence batters the hull. Fuel loss ")
            + to_string(realPct) + "% (" + to_string(loss) + "). Remaining "
            + to_string(pl.getFuel()) + ".";
    }
    else {
        double cargoFailProb = Config::STORM_BASE_CARGO_LOSS_MULTIPLIER * pl.getUpgrades().cargoLossMultiplier();
        bool loseCargo = (rand01() < cargoFailProb);
        if (loseCargo) {
            pl.loseCargo();
            out.gameOver = true;
            out.log = "[SPACE STORM] Cargo destroyed in the storm. Mission failed.";
        }
        else {
            out.log = "[SPACE STORM] Cargo narrowly preserved.";
        }
    }
    return out;
}

// Navigation Failure event
EventOutcome NavigationFailEvent::doExecute(Game& /*game*/, const HopContext& /*hop*/,
    const RoundConfig& /*rc*/, Player& pl) {
    EventOutcome out;
    int loss = max(
        Config::NAV_FAIL_MIN_LOSS,
        (int)std::lround(pl.getFuel() * Config::NAV_FAIL_PERCENT)
    );
    pl.setFuel(max(0, pl.getFuel() - loss));
    out.log = string("[NAVIGATION FAILURE] Course error forces corrections. Fuel loss ")
        + to_string(loss) + ". Remaining " + to_string(pl.getFuel()) + ".";
    return out;
}
