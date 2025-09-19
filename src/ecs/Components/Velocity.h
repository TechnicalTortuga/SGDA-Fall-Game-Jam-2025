#pragma once

#include "../Component.h"
#include "raylib.h"

class Velocity : public Component {
public:
    Velocity();
    Velocity(float vx, float vy, float vz = 0.0f);
    Velocity(const Vector3& vel);

    // Getters
    float GetX() const { return velocity.x; }
    float GetY() const { return velocity.y; }
    float GetZ() const { return velocity.z; }
    Vector3 GetVelocity() const { return velocity; }
    float GetSpeed() const; // Magnitude of velocity

    // Setters
    void SetX(float vx) { velocity.x = vx; }
    void SetY(float vy) { velocity.y = vy; }
    void SetZ(float vz) { velocity.z = vz; }
    void SetVelocity(float vx, float vy, float vz = 0.0f) {
        velocity.x = vx;
        velocity.y = vy;
        velocity.z = vz;
    }
    void SetVelocity(const Vector3& vel) { velocity = vel; }

    // Utility methods
    void Accelerate(float ax, float ay, float az = 0.0f) {
        velocity.x += ax;
        velocity.y += ay;
        velocity.z += az;
    }

    void Accelerate(const Vector3& acceleration) {
        velocity.x += acceleration.x;
        velocity.y += acceleration.y;
        velocity.z += acceleration.z;
    }

    void Stop() {
        velocity.x = 0.0f;
        velocity.y = 0.0f;
        velocity.z = 0.0f;
    }

    void Normalize(); // Make velocity unit length
    void Limit(float maxSpeed); // Limit velocity magnitude

private:
    Vector3 velocity;
};
