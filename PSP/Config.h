#pragma once

namespace Config {

    // Pradiniai resursai
    constexpr int STARTING_FUEL = 1000;
    constexpr int STARTING_CREDITS = 1000;

    // Raundų skaičius
    constexpr int MAX_ROUNDS = 5;

    // Event šansų ribos
    constexpr double MIN_EVENT_CHANCE = 0.05;
    constexpr double MAX_EVENT_CHANCE = 0.95;

    // RIZIKOS BUCKET ribos (naudojamos World::bucket)
    // <= RISK_LOW_MAX  -> Low
    // <= RISK_MED_MAX  -> Medium
    // > RISK_MED_MAX   -> High
    constexpr double RISK_LOW_MAX = 0.30;
    constexpr double RISK_MED_MAX = 0.50;

    // Round multiplikatoriai event rizikai
    constexpr double ROUND_MULT[1 + MAX_ROUNDS] = {
        0.0, // dummy
        0.8, // 1 round
        1.0, // 2
        1.2, // 3
        1.4, // 4
        1.6  // 5
    };

    // Round mission bonus
    constexpr int MISSION_BONUS[1 + MAX_ROUNDS] = {
        0,
        200,
        300,
        400,
        500,
        700
    };

    // Event baziniai šansai pagal bucket (1 raundui, vėliau dauginsim iš roundMult)
    constexpr double CERB_BASE_PROB_LOW = 0.10;
    constexpr double CERB_BASE_PROB_MED = 0.20;
    constexpr double CERB_BASE_PROB_HIGH = 0.35;

    constexpr double REAPER_BASE_PROB_LOW = 0.05;
    constexpr double REAPER_BASE_PROB_MED = 0.10;
    constexpr double REAPER_BASE_PROB_HIGH = 0.20;

    constexpr double STORM_BASE_PROB_LOW = 0.10;
    constexpr double STORM_BASE_PROB_MED = 0.20;
    constexpr double STORM_BASE_PROB_HIGH = 0.25;

    constexpr double NAV_FAIL_BASE_PROB_LOW = 0.05;
    constexpr double NAV_FAIL_BASE_PROB_MED = 0.08;
    constexpr double NAV_FAIL_BASE_PROB_HIGH = 0.12;

    // Cerberus reward'ai pagal tier
    constexpr int CERB_REWARD_LOW_MIN = 500;
    constexpr int CERB_REWARD_LOW_MAX = 900;

    constexpr int CERB_REWARD_MED_MIN = 300;
    constexpr int CERB_REWARD_MED_MAX = 650;

    constexpr int CERB_REWARD_HIGH_MIN = 200;
    constexpr int CERB_REWARD_HIGH_MAX = 400;

    // Cerberus pralaimėjimo kuro nuostoliai
    constexpr int CERB_DEFEAT_FUEL_LOSS = 120;

    // Reaper catch limitai ir bazės
    constexpr int REAPER_MAX_CATCH = 50;
    constexpr int REAPER_HIGH_START = 10;
    constexpr int REAPER_MED_START = 5;
    constexpr int REAPER_LOW_START = 3;

    constexpr int    REAPER_EVASIVE_BASE_FUEL = 80;
    constexpr double REAPER_EVASIVE_FUEL_MULT = 0.50;
    constexpr int    REAPER_EVASIVE_MIN_LOSS = 30;

    // Storm fuel nuostoliai
    constexpr int STORM_FUEL_LOSS_MIN_PCT = 5;
    constexpr int STORM_FUEL_LOSS_MAX_PCT = 15;

    // Navigation fail nuostoliai
    constexpr int    NAV_FAIL_MIN_LOSS = 40;
    constexpr double NAV_FAIL_PERCENT = 0.15;

    // UI / presentation
    constexpr int UI_PAUSE_MS = 800;

    // Cerberus event tuning
    constexpr int    CERB_TIER1_MAX_VICTORY_CHANCE = 35;
    constexpr int    CERB_TIER2_MAX_VICTORY_CHANCE = 55;
    constexpr double CERB_ENGINE_OVERDRIVE_EXTRA_OK_MULT = 0.35;
    constexpr double CERB_ENGINE_DEFAULT_EXTRA_OK_MULT = 0.50;
    constexpr double CERB_EXTRA_FAIL_MULT = 1.00;
    constexpr double CERB_BASE_CARGO_LOSS_PROB = 0.25;

    // Storm event tuning
    constexpr double STORM_EVENT_PROBABILITY = 0.60;
    constexpr double STORM_BASE_CARGO_LOSS_MULTIPLIER = 1.00;

}
