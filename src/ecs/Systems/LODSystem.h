#pragma once

#include "../System.h"
#include "../Components/LODComponent.h"
#include "../Components/MeshComponent.h"
#include "raylib.h"
#include <vector>
#include <memory>

class Entity;

/*
LODSystem - Level of Detail management system for ECS

Handles automatic LOD switching based on distance from camera.
Supports multiple LOD levels per entity and provides performance
optimization for distant objects while maintaining visual quality.
*/
class LODSystem : public System {
public:
    LODSystem();
    ~LODSystem();

    // System interface
    void Initialize() override;
    void Update(float deltaTime) override;
    void Shutdown() override;
    const char* GetName() const { return "LODSystem"; }

    // LOD management
    void RegisterLODEntity(Entity* entity);
    void UnregisterLODEntity(Entity* entity);

    // LOD configuration
    void SetGlobalLODDistances(float nearDistance, float mediumDistance, float farDistance);
    void SetCameraPosition(const Vector3& cameraPos) { cameraPosition_ = cameraPos; }
    void EnableLOD(bool enabled) { globalLODEnabled_ = enabled; }

    // LOD creation helpers
    void CreateLODLevelsForCube(Entity* entity, float size, const Color& color);
    void CreateLODLevelsForPyramid(Entity* entity, float baseSize, float height, const std::vector<Color>& faceColors);

    // Statistics
    int GetTotalLODSwitches() const { return totalLODSwitches_; }
    int GetActiveLODEntities() const { return activeLODEntities_.size(); }

private:
    // LOD processing
    void UpdateLODEntity(Entity* entity, float deltaTime);
    int CalculateOptimalLODIndex(const LODComponent* lodComp, float distance) const;
    float CalculateDistanceToCamera(const Vector3& entityPosition) const;

    // LOD mesh creation
    void CreateSimplifiedCubeMesh(Entity* entity, float size, const Color& color, int simplificationLevel);
    void CreateSimplifiedPyramidMesh(Entity* entity, float baseSize, float height, const std::vector<Color>& faceColors, int simplificationLevel);

    // Internal state
    Vector3 cameraPosition_ = {0.0f, 0.0f, 0.0f};
    bool globalLODEnabled_ = true;

    // LOD distances (configurable)
    float lodDistanceNear_ = 10.0f;    // High detail
    float lodDistanceMedium_ = 25.0f;  // Medium detail
    float lodDistanceFar_ = 50.0f;     // Low detail

    // Entity tracking
    std::vector<Entity*> activeLODEntities_;

    // Statistics
    int totalLODSwitches_ = 0;
    int frameLODSwitches_ = 0;

    // Helper methods
    LODComponent* GetLODComponent(Entity* entity) const;

private:
    bool initialized_;

    // Helper mesh creation functions
    void CreateFullDetailCube(MeshComponent* mesh, float halfSize, const Color& color);
    void CreateMediumDetailCube(MeshComponent* mesh, float halfSize, const Color& color);
    void CreateLowDetailCube(MeshComponent* mesh, float halfSize, const Color& color);
    void CreateFullDetailPyramid(MeshComponent* mesh, float baseSize, float height, const std::vector<Color>& faceColors);
    void CreateMediumDetailPyramid(MeshComponent* mesh, float baseSize, float height, const std::vector<Color>& faceColors);
    void CreateLowDetailPyramid(MeshComponent* mesh, float baseSize, float height, const std::vector<Color>& faceColors);
};
