#include "Position.h"

Position::Position()
    : position{0.0f, 0.0f, 0.0f}
{
}

Position::Position(float x, float y, float z)
    : position{x, y, z}
{
}

Position::Position(const Vector3& pos)
    : position(pos)
{
}
