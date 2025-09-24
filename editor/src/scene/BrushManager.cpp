#include "BrushManager.h"

BrushManager::BrushManager() {}

size_t BrushManager::CreateBrush(const Brush& brush) {
    brushes_.push_back(brush);
    return brushes_.size() - 1;
}

void BrushManager::RemoveBrush(size_t index) {
    if (index < brushes_.size()) {
        brushes_.erase(brushes_.begin() + index);
    }
}

const Brush& BrushManager::GetBrush(size_t index) const {
    static Brush defaultBrush = {PrimitiveType::Cube, {0, 0, 0}, {1, 1, 1}, 0.0f};
    if (index < brushes_.size()) {
        return brushes_[index];
    }
    return defaultBrush;
}

Brush& BrushManager::GetBrush(size_t index) {
    static Brush defaultBrush = {PrimitiveType::Cube, {0, 0, 0}, {1, 1, 1}, 0.0f};
    if (index < brushes_.size()) {
        return brushes_[index];
    }
    return defaultBrush;
}

size_t BrushManager::GetBrushCount() const {
    return brushes_.size();
}

void BrushManager::ClearBrushes() {
    brushes_.clear();
}

void BrushManager::SetBrushPosition(size_t index, Vector3 position) {
    if (index < brushes_.size()) {
        brushes_[index].position = position;
    }
}

void BrushManager::SetBrushSize(size_t index, Vector3 size) {
    if (index < brushes_.size()) {
        brushes_[index].size = size;
    }
}

void BrushManager::SetBrushRotation(size_t index, float rotation) {
    if (index < brushes_.size()) {
        brushes_[index].rotation = rotation;
    }
}

void BrushManager::SetBrushType(size_t index, PrimitiveType type) {
    if (index < brushes_.size()) {
        brushes_[index].type = type;
    }
}

std::vector<size_t> BrushManager::FindBrushesAtPosition(Vector3 worldPos, float tolerance) const {
    std::vector<size_t> result;

    for (size_t i = 0; i < brushes_.size(); ++i) {
        const auto& brush = brushes_[i];
        Vector3 brushCenter = brush.position;
        Vector3 halfSize = {
            brush.size.x * 0.5f,
            brush.size.y * 0.5f,
            brush.size.z * 0.5f
        };

        // Check if worldPos is within brush bounds
        if (worldPos.x >= brushCenter.x - halfSize.x - tolerance &&
            worldPos.x <= brushCenter.x + halfSize.x + tolerance &&
            worldPos.y >= brushCenter.y - halfSize.y - tolerance &&
            worldPos.y <= brushCenter.y + halfSize.y + tolerance &&
            worldPos.z >= brushCenter.z - halfSize.z - tolerance &&
            worldPos.z <= brushCenter.z + halfSize.z + tolerance) {
            result.push_back(i);
        }
    }

    return result;
}

std::vector<size_t> BrushManager::FindBrushesInBounds(Vector3 minBounds, Vector3 maxBounds) const {
    std::vector<size_t> result;

    for (size_t i = 0; i < brushes_.size(); ++i) {
        const auto& brush = brushes_[i];
        Vector3 brushMin = {
            brush.position.x - brush.size.x * 0.5f,
            brush.position.y - brush.size.y * 0.5f,
            brush.position.z - brush.size.z * 0.5f
        };
        Vector3 brushMax = {
            brush.position.x + brush.size.x * 0.5f,
            brush.position.y + brush.size.y * 0.5f,
            brush.position.z + brush.size.z * 0.5f
        };

        // Check if brush bounds overlap with query bounds
        if (brushMax.x >= minBounds.x && brushMin.x <= maxBounds.x &&
            brushMax.y >= minBounds.y && brushMin.y <= maxBounds.y &&
            brushMax.z >= minBounds.z && brushMin.z <= maxBounds.z) {
            result.push_back(i);
        }
    }

    return result;
}
