#pragma once
#include "../Component.h"
#include "raylib.h"

struct InterpolatedTransformComponent : public Component {
    // Current state
    Vector3 position = {0, 0, 0};
    Vector3 scale = {1, 1, 1};
    Quaternion rotation = {0, 0, 0, 1};

    // Network interpolation (client-side prediction)
    Vector3 targetPosition = {0, 0, 0};
    Quaternion targetRotation = {0, 0, 0, 1};
    Vector3 velocity = {0, 0, 0};           // For prediction
    Vector3 angularVelocity = {0, 0, 0};    // For rotation prediction

    // Interpolation state
    float interpolationTime = 0.0f;         // 0.0 to 1.0
    float interpolationDuration = 0.1f;     // Seconds to complete interpolation
    bool isInterpolating = false;

    // Server reconciliation
    Vector3 serverPosition = {0, 0, 0};     // Last server-confirmed position
    Quaternion serverRotation = {0, 0, 0, 1};
    uint32_t lastServerUpdate = 0;

    // Prediction error correction
    float positionErrorThreshold = 0.1f;    // Meters before correction
    float rotationErrorThreshold = 5.0f;    // Degrees before correction

    virtual const char* GetTypeName() const override { return "InterpolatedTransformComponent"; }
};
