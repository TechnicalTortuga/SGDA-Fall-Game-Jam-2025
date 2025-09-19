#include "Velocity.h"
#include <cmath>

Velocity::Velocity()
    : velocity{0.0f, 0.0f, 0.0f}
{
}

Velocity::Velocity(float vx, float vy, float vz)
    : velocity{vx, vy, vz}
{
}

Velocity::Velocity(const Vector3& vel)
    : velocity(vel)
{
}

float Velocity::GetSpeed() const
{
    return std::sqrt(velocity.x * velocity.x +
                     velocity.y * velocity.y +
                     velocity.z * velocity.z);
}

void Velocity::Normalize()
{
    float speed = GetSpeed();
    if (speed > 0.0f) {
        velocity.x /= speed;
        velocity.y /= speed;
        velocity.z /= speed;
    }
}

void Velocity::Limit(float maxSpeed)
{
    float speed = GetSpeed();
    if (speed > maxSpeed) {
        float ratio = maxSpeed / speed;
        velocity.x *= ratio;
        velocity.y *= ratio;
        velocity.z *= ratio;
    }
}
