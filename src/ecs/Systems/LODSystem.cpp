#include "LODSystem.h"
#include "../../utils/Logger.h"
#include "../Entity.h"
#include "../Components/Position.h"
#include "../Components/MeshComponent.h"
#include "../Systems/MeshSystem.h"
#include "../../core/Engine.h"
#include "raylib.h"
#include "raymath.h"
#include <algorithm>

LODSystem::LODSystem()
    : initialized_(false) {
    LOG_INFO("LODSystem created");
}

LODSystem::~LODSystem() {
    LOG_INFO("LODSystem destroyed");
}

void LODSystem::Initialize() {
    if (initialized_) return;

    LOG_INFO("LODSystem initialized with distances: Near=" +
             std::to_string(lodDistanceNear_) + ", Medium=" +
             std::to_string(lodDistanceMedium_) + ", Far=" +
             std::to_string(lodDistanceFar_));

    initialized_ = true;
}

void LODSystem::Update(float deltaTime) {
    if (!globalLODEnabled_) return;

    frameLODSwitches_ = 0;

    // Update all LOD entities
    for (auto* entity : activeLODEntities_) {
        if (entity && entity->IsActive()) {
            UpdateLODEntity(entity, deltaTime);
        }
    }

    // Log frame statistics if any switches occurred
    if (frameLODSwitches_ > 0) {
        LOG_DEBUG("LOD frame: " + std::to_string(frameLODSwitches_) + " switches, " +
                  std::to_string(activeLODEntities_.size()) + " entities");
    }

    // Update camera position for next frame (should be called by PlayerSystem)
    // This is a placeholder - camera position should be updated externally
}

void LODSystem::Shutdown() {
    activeLODEntities_.clear();
    LOG_INFO("LODSystem shutdown - Total switches: " + std::to_string(totalLODSwitches_));
}

void LODSystem::RegisterLODEntity(Entity* entity) {
    if (!entity) return;

    // Check if already registered
    auto it = std::find(activeLODEntities_.begin(), activeLODEntities_.end(), entity);
    if (it == activeLODEntities_.end()) {
        activeLODEntities_.push_back(entity);
        LOG_DEBUG("Registered LOD entity: " + std::to_string(entity->GetId()));
    }
}

void LODSystem::UnregisterLODEntity(Entity* entity) {
    if (!entity) return;

    auto it = std::remove(activeLODEntities_.begin(), activeLODEntities_.end(), entity);
    if (it != activeLODEntities_.end()) {
        activeLODEntities_.erase(it, activeLODEntities_.end());
        LOG_DEBUG("Unregistered LOD entity: " + std::to_string(entity->GetId()));
    }
}

void LODSystem::SetGlobalLODDistances(float nearDistance, float mediumDistance, float farDistance) {
    lodDistanceNear_ = nearDistance;
    lodDistanceMedium_ = mediumDistance;
    lodDistanceFar_ = farDistance;

    LOG_INFO("Updated LOD distances: Near=" + std::to_string(nearDistance) +
             ", Medium=" + std::to_string(mediumDistance) +
             ", Far=" + std::to_string(farDistance));
}

void LODSystem::CreateLODLevelsForCube(Entity* entity, float size, const Color& color) {
    if (!entity) return;

    auto* lodComp = GetLODComponent(entity);
    if (!lodComp) {
        entity->AddComponent<LODComponent>();
        lodComp = GetLODComponent(entity);
    }

    // Clear existing LOD levels
    lodComp->lodLevels.clear();

    // Create high detail (full cube)
    CreateSimplifiedCubeMesh(entity, size, color, 0); // Level 0 = full detail

    // Create medium detail (simplified)
    CreateSimplifiedCubeMesh(entity, size, color, 1); // Level 1 = medium detail

    // Create low detail (very simplified)
    CreateSimplifiedCubeMesh(entity, size, color, 2); // Level 2 = low detail

    // Configure LOD distances
    lodComp->lodLevels[0].distanceThreshold = 0.0f;     // Always use high detail when closest
    lodComp->lodLevels[0].levelName = "HIGH";

    lodComp->lodLevels[1].distanceThreshold = lodDistanceMedium_;
    lodComp->lodLevels[1].levelName = "MEDIUM";

    lodComp->lodLevels[2].distanceThreshold = lodDistanceFar_;
    lodComp->lodLevels[2].levelName = "LOW";

    lodComp->needsUpdate = true;

    LOG_DEBUG("Created 3 LOD levels for cube entity: " + std::to_string(entity->GetId()));
}

void LODSystem::CreateLODLevelsForPyramid(Entity* entity, float baseSize, float height, const std::vector<Color>& faceColors) {
    if (!entity) return;

    auto* lodComp = GetLODComponent(entity);
    if (!lodComp) {
        entity->AddComponent<LODComponent>();
        lodComp = GetLODComponent(entity);
    }

    // Clear existing LOD levels
    lodComp->lodLevels.clear();

    // Create high detail (full pyramid)
    CreateSimplifiedPyramidMesh(entity, baseSize, height, faceColors, 0);

    // Create medium detail (simplified)
    CreateSimplifiedPyramidMesh(entity, baseSize, height, faceColors, 1);

    // Create low detail (very simplified)
    CreateSimplifiedPyramidMesh(entity, baseSize, height, faceColors, 2);

    // Configure LOD distances
    lodComp->lodLevels[0].distanceThreshold = 0.0f;
    lodComp->lodLevels[0].levelName = "HIGH";

    lodComp->lodLevels[1].distanceThreshold = lodDistanceMedium_;
    lodComp->lodLevels[1].levelName = "MEDIUM";

    lodComp->lodLevels[2].distanceThreshold = lodDistanceFar_;
    lodComp->lodLevels[2].levelName = "LOW";

    lodComp->needsUpdate = true;

    LOG_DEBUG("Created 3 LOD levels for pyramid entity: " + std::to_string(entity->GetId()));
}

void LODSystem::UpdateLODEntity(Entity* entity, float deltaTime) {
    auto* lodComp = GetLODComponent(entity);
    if (!lodComp || !lodComp->isActive || lodComp->lodLevels.empty()) return;

    // Get entity position
    auto* position = entity->GetComponent<Position>();
    if (!position) return;

    // Calculate distance to camera
    float distance = CalculateDistanceToCamera(position->GetPosition());
    lodComp->currentDistance = distance;

    // Force update if needed
    if (lodComp->needsUpdate) {
        lodComp->needsUpdate = false;
    }

    // Calculate optimal LOD index
    int optimalLOD = CalculateOptimalLODIndex(lodComp, distance);

    // Switch LOD if needed
    if (optimalLOD != lodComp->currentLODIndex && lodComp->lodLevels[optimalLOD].isActive) {
        // Update the entity's mesh component to use the new LOD mesh
        auto* meshComp = entity->GetComponent<MeshComponent>();
        if (meshComp && optimalLOD < (int)lodComp->lodLevels.size()) {
            uint64_t newMeshEntityId = lodComp->lodLevels[optimalLOD].meshEntityId;

            // Here we would typically switch the mesh, but for now we'll just track the change
            LOG_DEBUG("LOD Switch: Entity " + std::to_string(entity->GetId()) +
                     " from " + lodComp->lodLevels[lodComp->currentLODIndex].levelName +
                     " to " + lodComp->lodLevels[optimalLOD].levelName +
                     " (distance: " + std::to_string(distance) + ")");

            lodComp->currentLODIndex = optimalLOD;
            lodComp->switchCount++;
            totalLODSwitches_++;
            frameLODSwitches_++;
        }
    }
}

int LODSystem::CalculateOptimalLODIndex(const LODComponent* lodComp, float distance) const {
    if (!lodComp || lodComp->lodLevels.empty()) return 0;

    // Find the appropriate LOD level based on distance
    for (size_t i = 0; i < lodComp->lodLevels.size(); ++i) {
        if (distance <= lodComp->lodLevels[i].distanceThreshold + lodComp->hysteresis) {
            return static_cast<int>(i);
        }
    }

    // If distance is greater than all thresholds, use the last (most simplified) LOD
    return static_cast<int>(lodComp->lodLevels.size() - 1);
}

float LODSystem::CalculateDistanceToCamera(const Vector3& entityPosition) const {
    return Vector3Distance(entityPosition, cameraPosition_);
}

void LODSystem::CreateSimplifiedCubeMesh(Entity* entity, float size, const Color& color, int simplificationLevel) {
    // For now, create a simple cube mesh regardless of simplification level
    // In a more advanced implementation, we would reduce triangle count based on level

    float halfSize = size * 0.5f;

    // Create a new entity for this LOD level's mesh
    Entity* meshEntity = engine_.CreateEntity();

    // Add mesh component
    auto* meshComp = meshEntity->AddComponent<MeshComponent>();

    // Create cube geometry based on simplification level
    if (simplificationLevel == 0) {
        // High detail: full cube with all 12 triangles
        CreateFullDetailCube(meshComp, halfSize, color);
    } else if (simplificationLevel == 1) {
        // Medium detail: simplified cube (6 triangles instead of 12)
        CreateMediumDetailCube(meshComp, halfSize, color);
    } else {
        // Low detail: very simplified (just 2 triangles for a flat representation)
        CreateLowDetailCube(meshComp, halfSize, color);
    }

    meshComp->meshName = "cube_lod_" + std::to_string(simplificationLevel);

    // Register this mesh entity with the LOD component
    auto* lodComp = GetLODComponent(entity);
    if (lodComp) {
        LODComponent::LODLevel level;
        level.meshEntityId = meshEntity->GetId();
        level.isActive = true;
        lodComp->lodLevels.push_back(level);
    }
}

void LODSystem::CreateSimplifiedPyramidMesh(Entity* entity, float baseSize, float height, const std::vector<Color>& faceColors, int simplificationLevel) {
    // Similar to cube, but for pyramids
    Entity* meshEntity = engine_.CreateEntity();

    auto* meshComp = meshEntity->AddComponent<MeshComponent>();

    if (simplificationLevel == 0) {
        // High detail pyramid
        CreateFullDetailPyramid(meshComp, baseSize, height, faceColors);
    } else if (simplificationLevel == 1) {
        // Medium detail pyramid
        CreateMediumDetailPyramid(meshComp, baseSize, height, faceColors);
    } else {
        // Low detail pyramid
        CreateLowDetailPyramid(meshComp, baseSize, height, faceColors);
    }

    meshComp->meshName = "pyramid_lod_" + std::to_string(simplificationLevel);

    // Register with LOD component
    auto* lodComp = GetLODComponent(entity);
    if (lodComp) {
        LODComponent::LODLevel level;
        level.meshEntityId = meshEntity->GetId();
        level.isActive = true;
        lodComp->lodLevels.push_back(level);
    }
}

// Helper mesh creation functions
void LODSystem::CreateFullDetailCube(MeshComponent* mesh, float halfSize, const Color& color) {
    // Full cube with all faces (12 triangles)
    mesh->vertices = {
        // Front face
        {{-halfSize, -halfSize, -halfSize}, {0, 0, -1}, {0, 0}, color},
        {{ halfSize, -halfSize, -halfSize}, {0, 0, -1}, {1, 0}, color},
        {{ halfSize,  halfSize, -halfSize}, {0, 0, -1}, {1, 1}, color},
        {{-halfSize,  halfSize, -halfSize}, {0, 0, -1}, {0, 1}, color},
        // Back face
        {{-halfSize, -halfSize,  halfSize}, {0, 0,  1}, {0, 0}, color},
        {{ halfSize, -halfSize,  halfSize}, {0, 0,  1}, {1, 0}, color},
        {{ halfSize,  halfSize,  halfSize}, {0, 0,  1}, {1, 1}, color},
        {{-halfSize,  halfSize,  halfSize}, {0, 0,  1}, {0, 1}, color}
    };

    mesh->triangles = {
        // Front
        {0, 1, 2}, {0, 2, 3},
        // Right
        {1, 5, 6}, {1, 6, 2},
        // Back
        {5, 4, 7}, {5, 7, 6},
        // Left
        {4, 0, 3}, {4, 3, 7},
        // Top
        {3, 2, 6}, {3, 6, 7},
        // Bottom
        {4, 5, 1}, {4, 1, 0}
    };
}

void LODSystem::CreateMediumDetailCube(MeshComponent* mesh, float halfSize, const Color& color) {
    // Medium detail: just front, right, and top faces (6 triangles)
    mesh->vertices = {
        {{-halfSize, -halfSize, -halfSize}, {0, 0, -1}, {0, 0}, color},
        {{ halfSize, -halfSize, -halfSize}, {0, 0, -1}, {1, 0}, color},
        {{ halfSize,  halfSize, -halfSize}, {0, 0, -1}, {1, 1}, color},
        {{-halfSize,  halfSize, -halfSize}, {0, 0, -1}, {0, 1}, color},
        {{-halfSize, -halfSize,  halfSize}, {0, 0,  1}, {0, 0}, color},
        {{ halfSize, -halfSize,  halfSize}, {0, 0,  1}, {1, 0}, color},
        {{ halfSize,  halfSize,  halfSize}, {0, 0,  1}, {1, 1}, color},
        {{-halfSize,  halfSize,  halfSize}, {0, 0,  1}, {0, 1}, color}
    };

    mesh->triangles = {
        // Front
        {0, 1, 2}, {0, 2, 3},
        // Right
        {1, 5, 6}, {1, 6, 2},
        // Top
        {3, 2, 6}, {3, 6, 7}
    };
}

void LODSystem::CreateLowDetailCube(MeshComponent* mesh, float halfSize, const Color& color) {
    // Low detail: just a simple quad (2 triangles)
    mesh->vertices = {
        {{-halfSize, -halfSize, 0}, {0, 0, -1}, {0, 0}, color},
        {{ halfSize, -halfSize, 0}, {0, 0, -1}, {1, 0}, color},
        {{ halfSize,  halfSize, 0}, {0, 0, -1}, {1, 1}, color},
        {{-halfSize,  halfSize, 0}, {0, 0, -1}, {0, 1}, color}
    };

    mesh->triangles = {
        {0, 1, 2}, {0, 2, 3}
    };
}

void LODSystem::CreateFullDetailPyramid(MeshComponent* mesh, float baseSize, float height, const std::vector<Color>& faceColors) {
    float halfBase = baseSize * 0.5f;
    Color color1 = faceColors.size() > 0 ? faceColors[0] : RED;
    Color color2 = faceColors.size() > 1 ? faceColors[1] : GREEN;
    Color color3 = faceColors.size() > 2 ? faceColors[2] : BLUE;
    Color color4 = faceColors.size() > 3 ? faceColors[3] : YELLOW;

    // Pyramid with base and 4 triangular faces
    mesh->vertices = {
        // Base
        {{-halfBase, 0, -halfBase}, {0, -1, 0}, {0, 0}, color1},
        {{ halfBase, 0, -halfBase}, {0, -1, 0}, {1, 0}, color1},
        {{ halfBase, 0,  halfBase}, {0, -1, 0}, {1, 1}, color1},
        {{-halfBase, 0,  halfBase}, {0, -1, 0}, {0, 1}, color1},
        // Apex
        {{0, height, 0}, {0, 1, 0}, {0.5f, 0.5f}, WHITE}
    };

    mesh->triangles = {
        // Base
        {0, 1, 2}, {0, 2, 3},
        // Front face
        {0, 1, 4},
        // Right face
        {1, 2, 4},
        // Back face
        {2, 3, 4},
        // Left face
        {3, 0, 4}
    };
}

void LODSystem::CreateMediumDetailPyramid(MeshComponent* mesh, float baseSize, float height, const std::vector<Color>& faceColors) {
    // Medium detail: just base and front face
    float halfBase = baseSize * 0.5f;
    Color color1 = faceColors.size() > 0 ? faceColors[0] : RED;

    mesh->vertices = {
        {{-halfBase, 0, -halfBase}, {0, -1, 0}, {0, 0}, color1},
        {{ halfBase, 0, -halfBase}, {0, -1, 0}, {1, 0}, color1},
        {{ halfBase, 0,  halfBase}, {0, -1, 0}, {1, 1}, color1},
        {{-halfBase, 0,  halfBase}, {0, -1, 0}, {0, 1}, color1},
        {{0, height, 0}, {0, 1, 0}, {0.5f, 0.5f}, WHITE}
    };

    mesh->triangles = {
        // Base
        {0, 1, 2}, {0, 2, 3},
        // Front face
        {0, 1, 4}
    };
}

void LODSystem::CreateLowDetailPyramid(MeshComponent* mesh, float baseSize, float height, const std::vector<Color>& faceColors) {
    // Low detail: just a triangle
    float halfBase = baseSize * 0.5f;
    Color color1 = faceColors.size() > 0 ? faceColors[0] : RED;

    mesh->vertices = {
        {{-halfBase, 0, 0}, {0, 0, -1}, {0, 0}, color1},
        {{ halfBase, 0, 0}, {0, 0, -1}, {1, 0}, color1},
        {{0, height, -halfBase}, {0, 1, 0}, {0.5f, 1}, WHITE}
    };

    mesh->triangles = {
        {0, 1, 2}
    };
}

LODComponent* LODSystem::GetLODComponent(Entity* entity) const {
    if (!entity) return nullptr;
    return entity->GetComponent<LODComponent>();
}
