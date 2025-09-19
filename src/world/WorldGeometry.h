#pragma once

#include "BSPTree.h"
#include <vector>
#include <unordered_map>
#include <memory>
#include "raylib.h"
#include "rlgl.h"
#include "../rendering/Skybox.h"

// WorldMaterial definition for static world geometry (avoiding raylib's Material conflict)
struct WorldMaterial {
    Color diffuseColor;    // Base color tint
    Texture2D texture;     // Cached texture resource (id==0 if none)
    float shininess;       // Reserved for future lighting
    bool hasTexture;       // Convenience flag

    WorldMaterial()
        : diffuseColor(WHITE), texture{0}, shininess(0.0f), hasTexture(false) {}

    explicit WorldMaterial(Color color)
        : diffuseColor(color), texture{0}, shininess(0.0f), hasTexture(false) {}
};

// WorldGeometry - Container for all compiled, static level data
// This class acts as the primary container for all static world data,
// completely independent of the ECS
class WorldGeometry {
public:
    WorldGeometry();
    ~WorldGeometry();

    // Core static data containers
    std::unique_ptr<BSPTree> bspTree;           // For physics queries and culling
    std::vector<Mesh> staticMeshes;             // Pre-batched meshes for efficient rendering
    std::unordered_map<int, WorldMaterial> materials; // Material definitions by surface ID

    // Skybox system
    std::unique_ptr<class Skybox> skybox;

    // Level metadata
    std::string levelName;
    Vector3 levelBoundsMin;
    Vector3 levelBoundsMax;
    Color skyColor;

    // Initialize with default values
    void Initialize();

    // Clear all data
    void Clear();

    // Query methods
    bool IsValid() const { return bspTree != nullptr; }
    bool ContainsPoint(const Vector3& point) const;
    float CastRay(const Vector3& origin, const Vector3& direction, float maxDistance = 1000.0f) const;
    std::vector<const Surface*> GetVisibleSurfaces(const Vector3& cameraPos) const;

    // BSP Tree access
    const BSPTree* GetBSPTree() const { return bspTree.get(); }
    BSPTree* GetBSPTree() { return bspTree.get(); }

    // Mesh and material access
    const std::vector<Mesh>& GetStaticMeshes() const { return staticMeshes; }
    const WorldMaterial* GetMaterial(int surfaceId) const;

    // Level info
    const std::string& GetLevelName() const { return levelName; }
    void SetLevelName(const std::string& name) { levelName = name; }

    Color GetSkyColor() const { return skyColor; }
    void SetSkyColor(Color color) { skyColor = color; }

    // Bounds queries
    Vector3 GetLevelBoundsMin() const { return levelBoundsMin; }
    Vector3 GetLevelBoundsMax() const { return levelBoundsMax; }
    void SetLevelBounds(const Vector3& min, const Vector3& max) {
        levelBoundsMin = min;
        levelBoundsMax = max;
    }

private:
    // Internal helper methods
    void CalculateBounds();
};
