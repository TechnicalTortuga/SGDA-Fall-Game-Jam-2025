#pragma once

#include <string>
#include <memory>

// Forward declarations
struct EventData;

// Base event data structure
struct EventData {
    virtual ~EventData() = default;
};

// Common event types
enum class EventType {
    // Game events
    GAME_START,
    GAME_END,
    GAME_PAUSE,
    GAME_RESUME,

    // Player events
    PLAYER_MOVE,
    PLAYER_SHOOT,
    PLAYER_HIT,
    PLAYER_DEATH,

    // Weapon events
    WEAPON_FIRE,
    WEAPON_RELOAD,

    // World events
    WORLD_LOAD,
    WORLD_UNLOAD,

    // Network events
    NETWORK_CONNECT,
    NETWORK_DISCONNECT,
    NETWORK_PLAYER_JOIN,
    NETWORK_PLAYER_LEAVE,

    // UI events
    UI_MENU_OPEN,
    UI_MENU_CLOSE,
    UI_BUTTON_CLICK,

    // Custom events can be added here
    CUSTOM_START = 1000
};

// Event structure
struct Event {
    EventType type;
    std::unique_ptr<EventData> data;
    uint64_t timestamp;

    Event(EventType t, std::unique_ptr<EventData> d = nullptr);
    Event(const Event& other);
    Event& operator=(const Event& other);
    Event(Event&&) = default;
    Event& operator=(Event&&) = default;
};

// Common event data structures
struct PlayerMoveData : public EventData {
    uint64_t playerId;
    float x, y, z;
    float velocityX, velocityY, velocityZ;

    PlayerMoveData(uint64_t id, float px, float py, float pz, float vx, float vy, float vz)
        : playerId(id), x(px), y(py), z(pz), velocityX(vx), velocityY(vy), velocityZ(vz) {}
};

struct WeaponFireData : public EventData {
    uint64_t playerId;
    uint64_t weaponId;
    float directionX, directionY, directionZ;

    WeaponFireData(uint64_t pid, uint64_t wid, float dx, float dy, float dz)
        : playerId(pid), weaponId(wid), directionX(dx), directionY(dy), directionZ(dz) {}
};

struct PlayerHitData : public EventData {
    uint64_t attackerId;
    uint64_t victimId;
    float damage;
    float hitX, hitY, hitZ;

    PlayerHitData(uint64_t attacker, uint64_t victim, float dmg, float hx, float hy, float hz)
        : attackerId(attacker), victimId(victim), damage(dmg), hitX(hx), hitY(hy), hitZ(hz) {}
};
