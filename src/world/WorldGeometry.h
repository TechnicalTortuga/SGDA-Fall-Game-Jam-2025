#pragma once

#include "BSPTree.h"
#include <vector>
#include <unordered_map>
#include <memory>
#include "raylib.h"
#include "rlgl.h"
#include "../rendering/Skybox.h"
#include "Brush.h"

// WorldMaterial definition for static world geometry (avoiding raylib's Material conflict)
struct WorldMaterial {
    Color diffuseColor;    // Base color tint
    Texture2D texture;     // Cached texture resource (id==0 if none)
    float shininess;       // Reserved for future lighting
    bool hasTexture;       // Convenience flag
    // Lightmap placeholders (non-breaking)
    Texture2D lightmap;    // Lightmap texture (id==0 if none)
    bool hasLightmap;      // Convenience flag

    WorldMaterial()
        : diffuseColor(WHITE), 
          texture{0,0,1,1,7}, // id,width,height,mipmaps,format (7=UNCOMPRESSED_R8G8B8A8)
          shininess(0.0f), 
          hasTexture(false),
          lightmap{0,0,0,0,0}, 
          hasLightmap(false) {}

    explicit WorldMaterial(Color color)
        : diffuseColor(color), 
          texture{0,0,1,1,7}, // id,width,height,mipmaps,format (7=UNCOMPRESSED_R8G8B8A8)
          shininess(0.0f), 
          hasTexture(false),
          lightmap{0,0,0,0,0}, 
          hasLightmap(false) {}
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
    struct StaticBatch {
        int materialId;
        std::vector<Vector3> positions;  // per-vertex positions (triangulated)
        std::vector<Vector2> uvs;        // per-vertex uvs
        std::vector<Color> colors;       // per-vertex colors (from face tint)
        std::vector<unsigned int> indices; // triangle indices into positions
    };
    std::vector<StaticBatch> batches;           // Pre-batched meshes for efficient rendering
    std::unordered_map<int, WorldMaterial> materials; // Material definitions by surface ID
    // Brush-based geometry (new pipeline)
    std::vector<Brush> brushes;
    std::vector<Face> faces; // flattened faces for BSP build

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
    // Brush-based queries
    std::vector<const Face*> GetVisibleFaces(const Camera3D& camera) const;

    // Build helpers (choose one pipeline)
    void BuildBSPFromFaces(const std::vector<Face>& inFaces);
    void BuildBSPFromBrushes(const std::vector<Brush>& inBrushes);

    // BSP Tree access
    const BSPTree* GetBSPTree() const { return bspTree.get(); }
    BSPTree* GetBSPTree() { return bspTree.get(); }

    // Mesh and material access
    const std::vector<StaticBatch>& GetBatches() const { return batches; }
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
    void ClearBatches();
public:
    // Build GPU batches by material from faces
    void BuildBatchesFromFaces(const std::vector<Face>& inFaces);
    
    // Update batch colors after materials are loaded
    void UpdateBatchColors();
};
