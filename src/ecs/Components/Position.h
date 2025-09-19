#pragma once

#include "../Component.h"
#include "raylib.h"

class Position : public Component {
public:
    Position();
    Position(float x, float y, float z = 0.0f);
    Position(const Vector3& pos);

    // Getters
    float GetX() const { return position.x; }
    float GetY() const { return position.y; }
    float GetZ() const { return position.z; }
    Vector3 GetPosition() const { return position; }

    // Setters
    void SetX(float x) { position.x = x; }
    void SetY(float y) { position.y = y; }
    void SetZ(float z) { position.z = z; }
    void SetPosition(float x, float y, float z = 0.0f) {
        position.x = x;
        position.y = y;
        position.z = z;
    }
    void SetPosition(const Vector3& pos) { position = pos; }

    // Utility methods
    void Move(float dx, float dy, float dz = 0.0f) {
        position.x += dx;
        position.y += dy;
        position.z += dz;
    }

    void Move(const Vector3& delta) {
        position.x += delta.x;
        position.y += delta.y;
        position.z += delta.z;
    }

private:
    Vector3 position;
};
