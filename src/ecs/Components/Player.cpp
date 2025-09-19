#include "Player.h"

Player::Player()
    : state_(PlayerState::ON_GROUND)
    , isRunning_(false)
    , isJumping_(false)
    , walkSpeed_(50.0f)
    , runSpeed_(100.0f)
    , crouchSpeed_(25.0f)
    , jumpForce_(15.0f)
    , health_(100)
    , maxHealth_(100)
    , standingHeight_(1.8f)
    , crouchingHeight_(0.9f)
    , noClip_(false)
{
}

void Player::SetState(PlayerState newState) {
    // Only allow state transitions that make sense
    if (state_ == newState) return;

    // Validate transitions
    switch (newState) {
        case PlayerState::ON_GROUND:
            // Can transition from any state to ground
            break;
        case PlayerState::IN_AIR:
            // Can transition from ground or crouching (jump)
            if (state_ == PlayerState::CROUCHING) {
                // Uncrouch when jumping
            }
            break;
        case PlayerState::CROUCHING:
            // Can only crouch when on ground
            if (state_ != PlayerState::ON_GROUND) {
                return; // Invalid transition
            }
            break;
    }

    state_ = newState;
}
