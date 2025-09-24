#pragma once
#include "../System.h"
#include "raylib.h"
#include <unordered_map>

class InterpolationSystem : public System {
public:
    InterpolationSystem();
    virtual ~InterpolationSystem();

    // System overrides
    void Initialize() override;
    void Update(float deltaTime) override;
    void Shutdown() override;

    // Prediction settings
    void SetMaxPredictionTime(float seconds) { maxPredictionTime_ = seconds; }
    void SetInterpolationSpeed(float speed) { interpolationSpeed_ = speed; }

    // Prediction management
    void StartPrediction(Entity* entity);
    void EndPrediction(Entity* entity);
    void ApplyServerCorrection(Entity* entity, const Vector3& serverPos, const Quaternion& serverRot);

    // Error correction
    float CalculatePositionError(const Vector3& predicted, const Vector3& actual) const;
    float CalculateRotationError(const Quaternion& predicted, const Quaternion& actual) const;

    // Interpolation management
    void SetTargetPosition(Entity* entity, const Vector3& targetPos, float duration = 0.1f);
    void SetTargetRotation(Entity* entity, const Quaternion& targetRot, float duration = 0.1f);

private:
    float maxPredictionTime_ = 0.2f;  // 200ms prediction window
    float interpolationSpeed_ = 5.0f; // Speed of error correction

    // Prediction state
    struct PredictionState {
        Vector3 originalPosition;
        Quaternion originalRotation;
        float predictionStartTime;
        bool isPredicting;
        std::vector<Vector3> positionHistory;
        std::vector<Quaternion> rotationHistory;
        std::vector<float> timestampHistory;
    };

    // Interpolation state
    struct InterpolationState {
        Vector3 startPosition;
        Vector3 targetPosition;
        Quaternion startRotation;
        Quaternion targetRotation;
        float interpolationTime;
        float interpolationDuration;
        bool isInterpolating;
    };

    std::unordered_map<Entity*, PredictionState> predictionStates_;
    std::unordered_map<Entity*, InterpolationState> interpolationStates_;

    // Helper methods
    void UpdatePrediction(Entity* entity, float deltaTime);
    void UpdateInterpolation(Entity* entity, float deltaTime);
    Vector3 PredictPosition(const std::vector<Vector3>& history, const std::vector<float>& timestamps, float futureTime);
    Quaternion PredictRotation(const std::vector<Quaternion>& history, const std::vector<float>& timestamps, float futureTime);
    void SmoothCorrection(Entity* entity, const Vector3& targetPos, const Quaternion& targetRot);
};
