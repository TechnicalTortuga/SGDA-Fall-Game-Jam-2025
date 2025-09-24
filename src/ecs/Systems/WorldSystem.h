#pragma once

#include "../System.h"
#include "../../world/WorldGeometry.h" // For World struct
#include "../../world/MapLoader.h"
#include "../../world/EntityFactory.h"
#include "../../world/BSPTreeSystem.h"
#include "../../world/MaterialValidator.h"
#include "../../ecs/Components/MeshComponent.h"
#include "../../ecs/Components/MaterialComponent.h"
#include "../../ecs/Components/TextureComponent.h"

#include "CollisionSystem.h"
#include "AssetSystem.h"
#include <vector>
#include <memory>
#include <unordered_map>

// Forward declarations
class RenderSystem;

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

    // World access - NEW: Primary interface for static world data (delegates to WorldGeometry)
    const World* GetWorld() const { return worldGeometry_ ? worldGeometry_->GetWorld() : nullptr; }
    World* GetWorld() { return worldGeometry_ ? worldGeometry_->GetWorld() : nullptr; }

    // LEGACY: WorldGeometry access (to be removed)
    const WorldGeometry* GetWorldGeometry() const { return worldGeometry_.get(); }
    WorldGeometry* GetWorldGeometry() { return worldGeometry_.get(); }

    // System integration
    void ConnectCollisionSystem(CollisionSystem* collisionSystem);
    void ConnectRenderSystem(RenderSystem* renderSystem);

    // Dynamic entities management - ONLY for dynamic objects (players, lights, doors, etc.)
    void CreateDynamicEntitiesFromMap(const MapData& mapData);
    void DestroyDynamicEntities();
    const std::vector<Entity*>& GetDynamicEntities() const { return dynamicEntities_; }

    // World queries - DELEGATED to WorldGeometry
    bool ContainsPoint(const Vector3& point) const;
    float CastRay(const Vector3& origin, const Vector3& direction, float maxDistance = 1000.0f) const;
    bool FindSpawnPoint(Vector3& spawnPoint) const;

    // Material ID mapping access for renderer
    const std::unordered_map<int, uint32_t>& GetMaterialIdMap() const { return materialIdMap_; }


private:
    // System references
    CollisionSystem* collisionSystem_;
    RenderSystem* renderSystem_;
    BSPTreeSystem* bspTreeSystem_;

    // World state - WorldGeometry now contains the World struct
    std::unique_ptr<WorldGeometry> worldGeometry_;

    MapLoader mapLoader_;
    MaterialValidator materialValidator_;
    bool mapLoaded_;

    std::vector<Entity*> dynamicEntities_;            // Only dynamic entities now

    // Entity creation
    std::unique_ptr<EntityFactory> entityFactory_;

    // Material ID mapping for world geometry (maps old material IDs to MaterialSystem IDs)
    std::unordered_map<int, uint32_t> materialIdMap_; // oldMaterialId -> materialSystemId

    // Texture loading state
    bool texturesNeedLoading_; // Whether textures need to be loaded when AssetSystem is ready

    // Persistent material ID tracking
    std::unordered_set<int> usedMaterialIds_; // Set of material IDs used in the current map

    // Map building pipeline
    void ProcessMapData(MapData& mapData);
    void BuildWorldGeometry(MapData& mapData);
    void BuildBSPTreeAfterMaterials();
    void CreateRenderBatches(const MapData& mapData);
    void LoadTexturesAndMaterials(const MapData& mapData);
    void LoadTexturesLegacy(const MapData& mapData); // Fallback when AssetSystem unavailable
    void LoadDeferredTextures(); // Load textures after AssetSystem is initialized
    void UpdateMaterialComponentWithTexture(int materialId, AssetSystem::TextureHandle textureHandle);
    void SetupSkybox(const MapData& mapData);

    // Default map creation
    MapData CreateTestMap();
    std::string ExportGeometryToYaml();
    void ExportGeometryToFile();

    // Default map creation helpers
    void AddTestGeometry(MapData& mapData);
    void CreateRoomGeometry(MapData& mapData);
    void CreateCorridorGeometry(MapData& mapData);
    void CreateSecondRoomGeometry(MapData& mapData);
    void AddTestDynamicEntity();

};
