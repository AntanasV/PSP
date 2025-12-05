#include "pch.h"
#include "CppUnitTest.h"

#include "Config.h"
#include "Domain.h"
#include "Game.h"
#include "Events.h"
#include "Utils.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace PSPUnitTests
{
    TEST_CLASS(GameLogicTests)
    {
    public:

        // 1) Paprasta pagalbinė logika – clamp
        TEST_METHOD(Utils_Clamp_WorksCorrectly)
        {
            Assert::AreEqual(5, clampi(5, 0, 10));
            Assert::AreEqual(0, clampi(-3, 0, 10));
            Assert::AreEqual(10, clampi(15, 0, 10));

            Assert::AreEqual(5.0, clampd(5.0, 0.0, 10.0));
            Assert::AreEqual(0.0, clampd(-1.0, 0.0, 10.0));
            Assert::AreEqual(10.0, clampd(11.0, 0.0, 10.0));
        }

        // 2) Player: kuro sumažėjimas ir apsauga nuo minus
        TEST_METHOD(Player_Fuel_Spend_And_Clamp)
        {
            Player p;
            p.setFuel(100);

            bool ok1 = p.spendFuel(30);   // turi pavykti
            bool ok2 = p.spendFuel(1000); // neturi leisti

            Assert::IsTrue(ok1);
            Assert::IsFalse(ok2);
            Assert::AreEqual(70, p.getFuel());

            // neigiamas set turi susiklampinti į 0
            p.setFuel(-50);
            Assert::AreEqual(0, p.getFuel());
        }

        // 3) Player: kreditų logika (add / spend / clamp)
        TEST_METHOD(Player_Credits_Add_And_Spend)
        {
            Player p;
            p.setCredits(200);
            p.addCredits(50);   // 250
            p.addCredits(-100); // 150

            Assert::AreEqual(150, p.getCredits());

            bool ok1 = p.spendCredits(50);   // 100
            bool ok2 = p.spendCredits(1000); // nepavyks

            Assert::IsTrue(ok1);
            Assert::IsFalse(ok2);
            Assert::AreEqual(100, p.getCredits());

            p.setCredits(-10);
            Assert::AreEqual(0, p.getCredits());
        }

        // 4) Upgrades: fight bonus ir storm shield / barriers multiplikatoriai
        TEST_METHOD(Upgrades_FightBonus_And_StormMultiplier)
        {
            Upgrades u;

            // default – be upgrade'ų
            Assert::AreEqual(0, u.fightBonus());
            Assert::AreEqual(1.0, u.stormFuelLossMultiplier(), 1e-9);

            u.buyWeaponsCalibration();   // +10
            u.setKineticBarriersLevel(2);// +12 ir storm *0.5
            u.enableEdiTargeting();      // +5
            u.enableStormShield();       // storm papildomai *0.75

            int expectedFight = 10 + 12 + 5; // 27
            Assert::AreEqual(expectedFight, u.fightBonus());

            double expectedStorm = 1.0;
            expectedStorm *= 0.5;   // barriers
            expectedStorm *= 0.75;  // shield
            Assert::AreEqual(expectedStorm, u.stormFuelLossMultiplier(), 1e-9);
        }

        // 5) World: rizikos bucket ribos
        TEST_METHOD(World_RiskBucket_Boundaries)
        {
            World w;

            // <= RISK_LOW_MAX -> Low
            Assert::IsTrue(w.bucket(Config::RISK_LOW_MAX) == RiskBucket::Low);

            // tarp LOW_MAX ir MED_MAX -> Medium
            Assert::IsTrue(w.bucket(Config::RISK_LOW_MAX + 0.01) == RiskBucket::Medium);
            Assert::IsTrue(w.bucket(Config::RISK_MED_MAX) == RiskBucket::Medium);

            // > RISK_MED_MAX -> High
            Assert::IsTrue(w.bucket(Config::RISK_MED_MAX + 0.01) == RiskBucket::High);
        }

        // 6) Game: round multipliers ir mission bonus pagal Config masyvus
        TEST_METHOD(Game_RoundMult_And_MissionBonus_From_Config)
        {
            Game g;

            for (int round = 1; round <= Config::MAX_ROUNDS; ++round)
            {
                double expectedMult = Config::ROUND_MULT[round];
                int expectedBonus = Config::MISSION_BONUS[round];

                Assert::AreEqual(expectedMult, g.roundMult(round), 1e-9);
                Assert::AreEqual(expectedBonus, g.missionBonus(round));
            }

            // out-of-range fallback'ai
            Assert::AreEqual(1.0, g.roundMult(0), 1e-9);
            Assert::AreEqual(Config::MISSION_BONUS[1], g.missionBonus(0));
        }

        // 7) NavigationFail event: kuro nuostolis ir neleidžia nukristi žemiau 0
        TEST_METHOD(Events_NavigationFail_ReducesFuel_Correctly)
        {
            Game game;                // čia naudojamas tik signatūrai
            HopContext hop;           // NavigationFail jo nenaudoja
            RoundConfig rc;           // taip pat nėra naudojamas
            Player p;

            p.setFuel(200);
            const int fuelBefore = p.getFuel();

            auto ev = EventFactory::create(EventType::NavigationFail);
            Assert::IsNotNull(ev.get());

            EventOutcome out = ev->execute(game, hop, rc, p);

            int expectedLoss = std::max(
                Config::NAV_FAIL_MIN_LOSS,
                (int)std::lround(fuelBefore * Config::NAV_FAIL_PERCENT)
            );
            int expectedFuelAfter = std::max(0, fuelBefore - expectedLoss);

            Assert::AreEqual(expectedFuelAfter, p.getFuel());
            Assert::IsFalse(out.gameOver);
            Assert::IsTrue(out.log.find("NAVIGATION FAILURE") != std::string::npos);
        }
    };
}
