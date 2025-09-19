#pragma once

#include "../System.h"
#include "../../world/WorldGeometry.h"
#include "../../world/MapLoader.h"
#include "../../rendering/WorldRenderer.h"
#include <vector>
#include <memory>

// Forward declaration for WorldRenderer
class WorldRenderer;

class WorldSystem : public System {
public:
    WorldSystem();
    ~WorldSystem() override = default;

    // System interface
    void Update(float deltaTime) override;
    void Render() override;
    void Initialize() override;
    void Shutdown() override;

    // Map loading and management - NEW ARCHITECTURE
    bool LoadMap(const std::string& mapPath);
    bool LoadDefaultMap();
    void UnloadMap();
    bool IsMapLoaded() const { return mapLoaded_; }

    // WorldGeometry access - NEW: Primary interface for static world data
    const WorldGeometry* GetWorldGeometry() const { return worldGeometry_.get(); }
    WorldGeometry* GetWorldGeometry() { return worldGeometry_.get(); }

    // WorldRenderer access - NEW: For static world rendering
    const WorldRenderer* GetWorldRenderer() const { return worldRenderer_.get(); }
    WorldRenderer* GetWorldRenderer() { return worldRenderer_.get(); }

    // Dynamic entities management - ONLY for dynamic objects (players, lights, doors, etc.)
    void CreateDynamicEntitiesFromMap(const MapData& mapData);
    void DestroyDynamicEntities();
    const std::vector<Entity*>& GetDynamicEntities() const { return dynamicEntities_; }

    // World queries - DELEGATED to WorldGeometry
    bool ContainsPoint(const Vector3& point) const;
    float CastRay(const Vector3& origin, const Vector3& direction, float maxDistance = 1000.0f) const;
    bool FindSpawnPoint(Vector3& spawnPoint) const;

    // Default map creation
    MapData CreateTestMap();
    void CreateFallbackMap();

private:
    std::unique_ptr<WorldGeometry> worldGeometry_;    // NEW: Container for all static world data
    std::unique_ptr<WorldRenderer> worldRenderer_;    // NEW: Renderer for static world
    MapLoader mapLoader_;                             // Simplified: Only parses .map files
    bool mapLoaded_;

    std::vector<Entity*> dynamicEntities_;            // RENAMED: Only dynamic entities now

    // NEW: Map building pipeline
    void ProcessMapData(const MapData& mapData);
    void BuildWorldGeometry(const MapData& mapData);
    void CreateRenderBatches(const MapData& mapData);
    void LoadTexturesAndMaterials(const MapData& mapData);
    void SetupSkybox(const MapData& mapData);

    // Dynamic entity creation helpers
    Entity* CreatePlayerSpawnEntity(const Vector3& position);
    Entity* CreateLightEntity(const Vector3& position, const Color& color, float intensity);
    Entity* CreateDoorEntity(const Vector3& position, const Vector3& size);

    // Default map creation helpers
    void AddDefaultTextures();
    void AddTestGeometry(MapData& mapData);
    void CreateRoomGeometry(MapData& mapData);
    void CreateCorridorGeometry(MapData& mapData);
    void CreateSecondRoomGeometry(MapData& mapData);
    void AddTestDynamicEntity();
};
