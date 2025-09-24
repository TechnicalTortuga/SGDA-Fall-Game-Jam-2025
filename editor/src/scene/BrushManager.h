#pragma once

#include <vector>
#include "../ui/CommandManager.h"

// Forward declaration
class MainWindow;

// Brush Manager - handles all brush operations
class BrushManager {
public:
    BrushManager();
    ~BrushManager() = default;

    // Brush operations
    size_t CreateBrush(const Brush& brush);
    void RemoveBrush(size_t index);
    const Brush& GetBrush(size_t index) const;
    Brush& GetBrush(size_t index);
    size_t GetBrushCount() const;
    void ClearBrushes();

    // Brush modification
    void SetBrushPosition(size_t index, Vector3 position);
    void SetBrushSize(size_t index, Vector3 size);
    void SetBrushRotation(size_t index, float rotation);
    void SetBrushType(size_t index, PrimitiveType type);

    // Utility functions
    std::vector<size_t> FindBrushesAtPosition(Vector3 worldPos, float tolerance = 0.1f) const;
    std::vector<size_t> FindBrushesInBounds(Vector3 minBounds, Vector3 maxBounds) const;

private:
    std::vector<Brush> brushes_;
};
