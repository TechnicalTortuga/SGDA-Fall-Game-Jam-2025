#include "WorldGeometry.h"
#include "../utils/Logger.h"
#include <algorithm>
#include "../rendering/Skybox.h"

#include "WorldGeometry.h"
#include "../utils/Logger.h"
#include <algorithm>
#include "../rendering/Skybox.h"

WorldGeometry::~WorldGeometry() = default;

WorldGeometry::WorldGeometry() {
    Initialize();
}

void WorldGeometry::Initialize() {
    bspTree = nullptr;
    staticMeshes.clear();
    materials.clear();
    levelName = "Untitled Level";
    levelBoundsMin = {0.0f, 0.0f, 0.0f};
    levelBoundsMax = {0.0f, 0.0f, 0.0f};
    skyColor = SKYBLUE;

        // Initialize skybox system
        skybox = std::make_unique<Skybox>();
}

void WorldGeometry::Clear() {
    if (bspTree) {
        bspTree->Clear();
        bspTree.reset();
    }
    staticMeshes.clear();
    materials.clear();
    levelName = "Untitled Level";
    levelBoundsMin = {0.0f, 0.0f, 0.0f};
    levelBoundsMax = {0.0f, 0.0f, 0.0f};
    skyColor = SKYBLUE;

    // Unload skybox resources
    if (skybox) {
        skybox->Unload();
    }
}

bool WorldGeometry::ContainsPoint(const Vector3& point) const {
    if (!bspTree) return false;
    return bspTree->ContainsPoint(point);
}

float WorldGeometry::CastRay(const Vector3& origin, const Vector3& direction, float maxDistance) const {
    if (!bspTree) return maxDistance;
    return bspTree->CastRay(origin, direction, maxDistance);
}

std::vector<const Surface*> WorldGeometry::GetVisibleSurfaces(const Vector3& cameraPos) const {
    std::vector<const Surface*> visibleSurfaces;
    if (!bspTree) return visibleSurfaces;

    bspTree->TraverseForRendering(cameraPos, visibleSurfaces);
    return visibleSurfaces;
}

const WorldMaterial* WorldGeometry::GetMaterial(int surfaceId) const {
    auto it = materials.find(surfaceId);
    return (it != materials.end()) ? &it->second : nullptr;
}

void WorldGeometry::CalculateBounds() {
    if (!bspTree) return;

    const auto& surfaces = bspTree->GetAllSurfaces();
    if (surfaces.empty()) return;

    levelBoundsMin = surfaces[0].start;
    levelBoundsMax = surfaces[0].start;

    for (const auto& surface : surfaces) {
        // Update bounds with start point
        levelBoundsMin.x = std::min(levelBoundsMin.x, surface.start.x);
        levelBoundsMin.y = std::min(levelBoundsMin.y, surface.start.y);
        levelBoundsMin.z = std::min(levelBoundsMin.z, surface.start.z);

        levelBoundsMax.x = std::max(levelBoundsMax.x, surface.start.x);
        levelBoundsMax.y = std::max(levelBoundsMax.y, surface.start.y);
        levelBoundsMax.z = std::max(levelBoundsMax.z, surface.start.z);

        // Update bounds with end point
        levelBoundsMin.x = std::min(levelBoundsMin.x, surface.end.x);
        levelBoundsMin.y = std::min(levelBoundsMin.y, surface.end.y);
        levelBoundsMin.z = std::min(levelBoundsMin.z, surface.end.z);

        levelBoundsMax.x = std::max(levelBoundsMax.x, surface.end.x);
        levelBoundsMax.y = std::max(levelBoundsMax.y, surface.end.y);
        levelBoundsMax.z = std::max(levelBoundsMax.z, surface.end.z);

        // Include floor and ceiling heights
        levelBoundsMin.y = std::min({levelBoundsMin.y, surface.floorHeight, surface.ceilingHeight});
        levelBoundsMax.y = std::max({levelBoundsMax.y, surface.floorHeight, surface.ceilingHeight});
    }
}

// Skybox logic is now handled by the Skybox class.



