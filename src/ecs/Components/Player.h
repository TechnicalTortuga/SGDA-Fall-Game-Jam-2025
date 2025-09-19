#pragma once

#include "../Component.h"
#include "raylib.h"
#include <algorithm>

// Player state enumeration for physics and animation
enum class PlayerState {
    ON_GROUND,      // Player is standing/walking on ground
    IN_AIR,         // Player is jumping or falling
    CROUCHING       // Player is crouching (smaller hitbox)
};

/**
 * Player component containing player-specific data and state
 */
class Player : public Component {
public:
    Player();

    // State management
    PlayerState GetState() const { return state_; }
    void SetState(PlayerState newState);
    bool IsOnGround() const { return state_ == PlayerState::ON_GROUND; }
    bool IsInAir() const { return state_ == PlayerState::IN_AIR; }
    bool IsCrouching() const { return state_ == PlayerState::CROUCHING; }

    // Movement properties
    float GetWalkSpeed() const { return walkSpeed_; }
    float GetRunSpeed() const { return runSpeed_; }
    float GetJumpForce() const { return jumpForce_; }
    float GetCrouchSpeed() const { return crouchSpeed_; }

    void SetWalkSpeed(float speed) { walkSpeed_ = speed; }
    void SetRunSpeed(float speed) { runSpeed_ = speed; }
    void SetJumpForce(float force) { jumpForce_ = force; }
    void SetCrouchSpeed(float speed) { crouchSpeed_ = speed; }

    // Health and status
    int GetHealth() const { return health_; }
    int GetMaxHealth() const { return maxHealth_; }
    void SetHealth(int health) { health_ = std::max(0, std::min(health, maxHealth_)); }
    void SetMaxHealth(int maxHealth) { maxHealth_ = maxHealth; }
    void TakeDamage(int damage) { SetHealth(health_ - damage); }
    void Heal(int amount) { SetHealth(health_ + amount); }
    bool IsAlive() const { return health_ > 0; }

    // Crouching mechanics
    float GetStandingHeight() const { return standingHeight_; }
    float GetCrouchingHeight() const { return crouchingHeight_; }
    void SetStandingHeight(float height) { standingHeight_ = height; }
    void SetCrouchingHeight(float height) { crouchingHeight_ = height; }

    // Input state
    bool IsRunning() const { return isRunning_; }
    bool IsJumping() const { return isJumping_; }
    void SetRunning(bool running) { isRunning_ = running; }
    void SetJumping(bool jumping) { isJumping_ = jumping; }

    // Debug/cheat features
    bool HasNoClip() const { return noClip_; }
    void SetNoClip(bool enabled) { noClip_ = enabled; }
    void ToggleNoClip() { noClip_ = !noClip_; }

private:
    // State
    PlayerState state_;
    bool isRunning_;
    bool isJumping_;

    // Movement
    float walkSpeed_;
    float runSpeed_;
    float crouchSpeed_;
    float jumpForce_;

    // Health
    int health_;
    int maxHealth_;

    // Dimensions
    float standingHeight_;
    float crouchingHeight_;

    // Debug
    bool noClip_;
};
