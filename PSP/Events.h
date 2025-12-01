#pragma once

#include <string>
#include <memory>
#include "Domain.h"

// Forward deklaracija, kad išvengtume ciklinės priklausomybės
struct Game;

enum class EventType { None, Reaper, Cerberus, Storm, NavigationFail };

struct EventOutcome {
    bool gameOver = false;
    std::string log;
};

// Abstrakti bazinė event klasė (abstraction + polymorphism + template method)
class GameEvent {
public:
    virtual ~GameEvent() = default;

    // Template Method: bendras įėjimo taškas visiems eventams
    EventOutcome execute(Game& game, const HopContext& hop,
        const RoundConfig& rc, Player& pl);

protected:
    virtual EventOutcome doExecute(Game& game, const HopContext& hop,
        const RoundConfig& rc, Player& pl) = 0;
};

// Konkrečių eventų klasės (inheritance)
class CerberusEvent : public GameEvent {
protected:
    EventOutcome doExecute(Game& game, const HopContext& hop,
        const RoundConfig& rc, Player& pl) override;
};

class ReaperEvent : public GameEvent {
protected:
    EventOutcome doExecute(Game& game, const HopContext& hop,
        const RoundConfig& rc, Player& pl) override;
};

class StormEvent : public GameEvent {
protected:
    EventOutcome doExecute(Game& game, const HopContext& hop,
        const RoundConfig& rc, Player& pl) override;
};

class NavigationFailEvent : public GameEvent {
protected:
    EventOutcome doExecute(Game& game, const HopContext& hop,
        const RoundConfig& rc, Player& pl) override;
};

// Creational pattern: Factory
class EventFactory {
public:
    static std::unique_ptr<GameEvent> create(EventType type);
};
