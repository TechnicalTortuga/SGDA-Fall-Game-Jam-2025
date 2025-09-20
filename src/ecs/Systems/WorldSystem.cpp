#include "WorldSystem.h"
#include "../../rendering/Skybox.h"
#include "../Entity.h"
#include "../Components/Position.h"
#include "../Components/Sprite.h"
#include "../Components/Collidable.h"
#include "../Components/Velocity.h"
#include "../Components/TransformComponent.h"
#include "../Systems/MeshSystem.h"
#include "../../core/Engine.h"
#include "../../rendering/TextureManager.h"
#include "../../utils/Logger.h"
#include "../../utils/PathUtils.h"
#include <unordered_map>

WorldSystem::WorldSystem()
    : worldGeometry_(std::make_unique<WorldGeometry>())
    , worldRenderer_(std::make_unique<WorldRenderer>())
    , mapLoaded_(false)
{
    LOG_INFO("WorldSystem constructor called");
}

void WorldSystem::Initialize() {
    LOG_INFO("WorldSystem initialize called");
    LOG_INFO("WorldSystem initialized (stage vs actors architecture)");

    // Initialize the WorldRenderer with the WorldGeometry
    worldRenderer_->SetWorldGeometry(worldGeometry_.get());

    LoadDefaultMap();
}

bool WorldSystem::LoadDefaultMap() {
    LOG_INFO("Loading default test map");

    // Unload current map
    UnloadMap();

    // Try to load the test map file using executable-relative path
    std::string exeDir = Utils::GetExecutableDir();
    std::string testMapPath = exeDir + "/assets/maps/test_level.map";
    MapData mapData = mapLoader_.LoadMap(testMapPath);

    // If that fails, try CWD-relative
    if (mapData.faces.empty()) {
        LOG_WARNING("Failed to load map from exe-relative path, trying CWD-relative");
        testMapPath = "assets/maps/test_level.map";
        mapData = mapLoader_.LoadMap(testMapPath);
    }

    // If that fails, try project-root relative (when running from build/bin)
    if (mapData.faces.empty()) {
        LOG_WARNING("Failed to load map from CWD-relative path, trying project-root relative");
        testMapPath = exeDir + "/../../assets/maps/test_level.map";
        mapData = mapLoader_.LoadMap(testMapPath);
    }

    if (!mapData.faces.empty()) {
        // Process the loaded map data through the new pipeline
        ProcessMapData(mapData);
        mapLoaded_ = true;
        LOG_INFO("Test map loaded successfully from file");
        return true;
    }

    // If file loading fails, create test map data directly
    LOG_WARNING("Test map file not found; creating test map data");
    MapData testMapData = CreateTestMap();
    ProcessMapData(testMapData);

    mapLoaded_ = true;
    LOG_INFO("Test map created and processed successfully");
    return true;
}

void WorldSystem::Shutdown() {
    UnloadMap();
    LOG_INFO("WorldSystem shutdown complete");
}

void WorldSystem::Update(float deltaTime) {
    // Update dynamic world elements

    // Rotate only the pyramid entity (in Room 3, X=-15), not the cube (in Room 2, X=21)
    for (Entity* entity : dynamicEntities_) {
        if (entity && entity->IsActive()) {
            auto position = entity->GetComponent<Position>();
            auto transform = entity->GetComponent<TransformComponent>();
            if (transform && position) {
                // Only rotate entities in Room 3 (pyramid), not Room 2 (cube)
                if (position->GetX() < 0.0f) { // Room 3 is at negative X coordinates
                    // Rotate at 180 degrees per second around Y axis
                    float rotationSpeed = 180.0f * 0.01745329252f; // 180 degrees to radians
                    Quaternion rotationDelta = QuaternionFromAxisAngle({0, 1, 0}, rotationSpeed * deltaTime);
                    transform->rotation = QuaternionMultiply(transform->rotation, rotationDelta);
                }
            }
        }
    }
}

void WorldSystem::Render() {
    // World rendering is handled by the RenderSystem
    // This could be used for debug visualization
}

bool WorldSystem::LoadMap(const std::string& mapPath) {
    LOG_INFO("Loading map: " + mapPath);

    // Unload current map
    UnloadMap();

    // Load and parse map file into raw MapData with path resolution
    MapData mapData = mapLoader_.LoadMap(mapPath);

    // If direct path fails, try executable-relative
    if (mapData.faces.empty()) {
        std::string exeDir = Utils::GetExecutableDir();
        std::string exeRelativePath = exeDir + "/" + mapPath;
        LOG_WARNING("Direct map path failed, trying exe-relative: " + exeRelativePath);
        mapData = mapLoader_.LoadMap(exeRelativePath);
    }

    if (mapData.faces.empty()) {
        LOG_ERROR("Failed to load map: " + mapPath + " - No surfaces found");
        return false;
    }

    // Process the raw MapData through the new building pipeline
    ProcessMapData(mapData);
    mapLoaded_ = true;

    LOG_INFO("Map loaded successfully from: " + mapPath +
             " (Faces: " + std::to_string(mapData.faces.size()) +
             ", Dynamic Entities: " + std::to_string(dynamicEntities_.size()) + ")");

    // Connect collision system with the newly loaded BSP tree
    if (collisionSystem_ && worldGeometry_) {
        collisionSystem_->SetBSPTree(worldGeometry_->GetBSPTree());
        LOG_INFO("Collision system connected to BSP tree after map loading");
    }

    return true;
}


void WorldSystem::UnloadMap() {
    if (mapLoaded_) {
        DestroyDynamicEntities();
        worldGeometry_->Clear();
        mapLoaded_ = false;
        LOG_INFO("Map unloaded - WorldGeometry and dynamic entities cleared");
    }
}

// NEW ARCHITECTURE: Main processing pipeline
void WorldSystem::ProcessMapData(const MapData& mapData) {
    LOG_INFO("Processing MapData through NEW pipeline...");

    // Step 1: Build the WorldGeometry (static world data)
    BuildWorldGeometry(mapData);

    // Step 2: Create render batches and materials
    CreateRenderBatches(mapData);
    LoadTexturesAndMaterials(mapData);

    // Step 4: Setup skybox
    SetupSkybox(mapData);

    // Step 5: Create dynamic entities (players, lights, doors, etc.)
    CreateDynamicEntitiesFromMap(mapData);

    LOG_INFO("MapData processing complete");
}

// NEW: Build WorldGeometry from MapData
void WorldSystem::BuildWorldGeometry(const MapData& mapData) {
    LOG_INFO("Building WorldGeometry from MapData");

    // Clear any existing data
    worldGeometry_->Clear();

    // Set basic level info
    worldGeometry_->SetLevelName(mapData.name);
    worldGeometry_->SetSkyColor(mapData.skyColor);

    // Create and build BSP tree (prefer brushes; otherwise faces)
    if (!mapData.brushes.empty()) {
        worldGeometry_->BuildBSPFromBrushes(mapData.brushes);
        size_t faceCount = 0; for (const auto& b : mapData.brushes) faceCount += b.faces.size();
        LOG_INFO("WorldGeometry built from brushes with ~" + std::to_string(faceCount) + " faces");
    } else if (!mapData.faces.empty()) {
        worldGeometry_->BuildBSPFromFaces(mapData.faces);
        LOG_INFO("WorldGeometry built with " + std::to_string(mapData.faces.size()) + " faces");
    } else {
        LOG_WARNING("No brushes or faces in MapData; WorldGeometry will be empty");
    }
}

// NEW: Create render batches (placeholder for now)
void WorldSystem::CreateRenderBatches(const MapData& mapData) {
    LOG_INFO("Creating render batches");

    // Placeholder: batching for faces (future)
    std::unordered_map<int, int> faceCounts;
    for (const auto& f : mapData.faces) {
        faceCounts[f.materialId]++;
    }
    LOG_INFO("Counted faces across materials: " + std::to_string(faceCounts.size()) + " groups");
}

// NEW: Load textures and create materials
void WorldSystem::LoadTexturesAndMaterials(const MapData& mapData) {
    LOG_INFO("Loading textures and creating materials");

    // Default materials if textures fail to load
    WorldMaterial defaultWallMaterial(GRAY);
    defaultWallMaterial.shininess = 10.0f;

    WorldMaterial defaultFloorMaterial(DARKGRAY);
    defaultFloorMaterial.shininess = 5.0f;

    WorldMaterial defaultCeilingMaterial(LIGHTGRAY);
    defaultCeilingMaterial.shininess = 5.0f;

    // Pre-create default materials (expand as needed for new textures)
    worldGeometry_->materials[0] = defaultWallMaterial;    // Wall material (id: 0)
    worldGeometry_->materials[1] = defaultFloorMaterial;   // Floor material (id: 1)
    worldGeometry_->materials[2] = defaultCeilingMaterial; // Ceiling material (id: 2)
    worldGeometry_->materials[3] = WorldMaterial(ORANGE);  // Orange material (id: 3)

    // Load textures and update materials
    for (const auto& textureInfo : mapData.textures) {
        // Skip if material ID is invalid
        if (textureInfo.index < 0) {
            LOG_WARNING("Skipping texture with invalid material ID: " + std::to_string(textureInfo.index));
            continue;
        }

        // Load via TextureManager (executable-relative)
        std::string texturePath = "assets/textures/" + textureInfo.name;
        Texture2D texture = TextureManager::Get().Load(texturePath);

        if (texture.id != 0) {
            // Update the existing material with the texture
            WorldMaterial& material = worldGeometry_->materials[textureInfo.index];
            material.texture = texture;
            material.hasTexture = true;

            // Set appropriate shininess based on material type
            if (textureInfo.index == 0) { // Wall
                material.shininess = 10.0f;
            } else { // Floor or ceiling
                material.shininess = 5.0f;
            }

            LOG_INFO("Loaded texture " + textureInfo.name + " for material ID: " + std::to_string(textureInfo.index) +
                     " -> Raylib texture ID: " + std::to_string(material.texture.id));
        } else {
            LOG_WARNING("Failed to load texture: " + texturePath + ", using default material for ID: " + std::to_string(textureInfo.index));
        }
    }

    LOG_INFO("Initialized " + std::to_string(worldGeometry_->materials.size()) + " materials");

    // Validate material state before rebuilding
    for (const auto& [id, material] : worldGeometry_->materials) {
        LOG_INFO("Material " + std::to_string(id) + " validation: hasTexture=" +
                 std::to_string(material.hasTexture) + " texture.id=" + std::to_string(material.texture.id));
    }

    // Rebuild batches now that materials have textures loaded
    if (!worldGeometry_->faces.empty()) {
        LOG_INFO("Rebuilding batches with loaded textures");
        worldGeometry_->BuildBatchesFromFaces(worldGeometry_->faces);
    }

    // Log the final material setup
    for (const auto& [id, material] : worldGeometry_->materials) {
        LOG_DEBUG("Material ID: " + std::to_string(id) +
                 " hasTexture: " + std::to_string(material.hasTexture) +
                 " textureID: " + std::to_string(material.texture.id));
    }
}

// NEW: Setup skybox with robust loading and fallbacks
void WorldSystem::SetupSkybox(const MapData& mapData) {
    LOG_INFO("Skybox: setting up with cubemap support");

    // Try to load skybox using the new cubemap system
    std::string skyboxPath = "assets/textures/skyboxcubemaps/cubemap_cloudy&blue.png";
    if (worldGeometry_->skybox->LoadFromFile(skyboxPath)) {
        LOG_INFO("Skybox: loaded cubemap from: " + skyboxPath);
        return;
    }
    LOG_WARNING("Skybox: failed to load primary cubemap; trying fallbacks");
    std::vector<std::string> fallbackPaths = {
        "assets/textures/skybox.png",
        "assets/textures/cubemap.png",
        "assets/skybox/cloudy.png"
    };
    for (const auto& path : fallbackPaths) {
        if (worldGeometry_->skybox->LoadFromFile(path)) {
            LOG_INFO("Skybox: loaded fallback cubemap from: " + path);
            return;
        }
    }
    // If all file loading fails, create a test skybox
    LOG_WARNING("Skybox: all cubemap files failed; creating test skybox");
    if (worldGeometry_->skybox->LoadTestSkybox()) {
        LOG_INFO("Skybox: test cubemap created");
    } else {
        LOG_ERROR("Skybox: failed to create test skybox");
    }
}

// NEW: Create dynamic entities only
void WorldSystem::CreateDynamicEntitiesFromMap(const MapData& mapData) {
    LOG_INFO("Creating dynamic entities from MapData");

    // Clear existing dynamic entities
    DestroyDynamicEntities();

    // For now, create a test dynamic entity at spawn point
    // In a full implementation, this would parse entity definitions from the map
    Vector3 spawnPoint = {0.0f, 2.0f, 0.0f};
    AddTestDynamicEntity();

    LOG_INFO("Created " + std::to_string(dynamicEntities_.size()) + " dynamic entities");
}

// RENAMED: Destroy dynamic entities only
void WorldSystem::DestroyDynamicEntities() {
    for (Entity* entity : dynamicEntities_) {
        if (entity) {
            GetEngine()->DestroyEntity(entity);
        }
    }
    dynamicEntities_.clear();
    LOG_INFO("Dynamic entities destroyed");
}


// DELEGATED: Point containment check
bool WorldSystem::ContainsPoint(const Vector3& point) const {
    return worldGeometry_ ? worldGeometry_->ContainsPoint(point) : false;
}

// DELEGATED: Ray casting
float WorldSystem::CastRay(const Vector3& origin, const Vector3& direction, float maxDistance) const {
    return worldGeometry_ ? worldGeometry_->CastRay(origin, direction, maxDistance) : maxDistance;
}

bool WorldSystem::FindSpawnPoint(Vector3& spawnPoint) const {
    // For now, use a default spawn point
    // In a full implementation, this would read spawn points from the map
    spawnPoint = {0.0f, 2.0f, 0.0f};
    return true;
}

void WorldSystem::ConnectCollisionSystem(CollisionSystem* collisionSystem) {
    collisionSystem_ = collisionSystem;

    // If map is already loaded, connect immediately
    if (mapLoaded_ && worldGeometry_) {
        collisionSystem_->SetBSPTree(worldGeometry_->GetBSPTree());
        LOG_INFO("Collision system connected to existing BSP tree");
    }
}

MapData WorldSystem::CreateTestMap() {
    LOG_INFO("Creating test map data");

    // Create map data structure
    MapData mapData;
    mapData.name = "Test Map - Stage vs Actors";
    mapData.skyColor = SKYBLUE;
    mapData.floorHeight = 0.0f;
    mapData.ceilingHeight = 8.0f;

    // Add basic textures (without loading them - just metadata)
    mapData.textures.emplace_back("devtextures/Dark/proto_wall_dark.png", 0);
    mapData.textures.emplace_back("devtextures/Light/proto_1024_light.png", 1);
    mapData.textures.emplace_back("devtextures/Green/proto_1024_green.png", 2);
    mapData.textures.emplace_back("devtextures/Orange/proto_1024_orange.png", 3);

    // Create geometry and add to map data
    AddTestGeometry(mapData);

    LOG_INFO("Test map data created with " + std::to_string(mapData.faces.size()) + " faces");
    return mapData;
}

// Add geometry for the test map - Enhanced with slopes, platforms, and larger rooms
void WorldSystem::AddTestGeometry(MapData& mapData) {
    LOG_INFO("Adding enhanced multi-room geometry with slopes and platforms to test map");

    const float wallHeight = 8.0f;
    const float floorY = 0.0f;
    const float ceilingY = 8.0f;

    // === ROOM 1 (Starting area): -5 to 5 in X, -5 to 5 in Z, NO CEILING ===
    
    // Helper lambdas to create faces from legacy params
    auto AddHorizontalFace = [&](const Vector3& start, const Vector3& end, float y, int mat, Color tint){
        Face f; f.materialId = mat; f.tint = tint;
        Vector3 p1{start.x, y, start.z};
        Vector3 p2{end.x,   y, start.z};
        Vector3 p3{end.x,   y, end.z};
        Vector3 p4{start.x, y, end.z};
        // If this is a floor (assume floorY), wind CCW as seen from above to get +Y normal
        if (fabsf(y - floorY) < 0.001f) {
            f.vertices = {p1, p4, p3, p2};
        } else {
            // Ceiling: keep winding to get -Y normal
            f.vertices = {p1, p2, p3, p4};
        }
        f.RecalculateNormal();
        mapData.faces.push_back(f);
    };
    auto AddVerticalWall = [&](const Vector3& start, const Vector3& end, float height, int mat, Color tint){
        Face f; f.materialId = mat; f.tint = tint;
        float bottomY = start.y; float topY = bottomY + height;
        Vector3 bottomLeft{start.x, bottomY, start.z};
        Vector3 bottomRight{end.x,   bottomY, end.z};
        Vector3 topRight{end.x,   topY, end.z};
        Vector3 topLeft{start.x, topY, start.z};
        f.vertices = {bottomLeft, bottomRight, topRight, topLeft}; f.RecalculateNormal();
        mapData.faces.push_back(f);
    };
    
    // Helper to create solid boxes with all 6 faces and correct winding
    auto AddSolidBox = [&](const Vector3& minCorner, const Vector3& maxCorner, int mat, Color tint) {
        LOG_INFO("AddSolidBox DEBUG: Creating box with material ID " + std::to_string(mat) + " tint (" + 
                 std::to_string(tint.r) + "," + std::to_string(tint.g) + "," + std::to_string(tint.b) + ")");
        float minX = minCorner.x, minY = minCorner.y, minZ = minCorner.z;
        float maxX = maxCorner.x, maxY = maxCorner.y, maxZ = maxCorner.z;
        
        // Top face (+Y normal)
        Face topFace; topFace.materialId = mat; topFace.tint = tint;
        topFace.vertices = {{minX, maxY, minZ}, {minX, maxY, maxZ}, {maxX, maxY, maxZ}, {maxX, maxY, minZ}};
        topFace.RecalculateNormal(); mapData.faces.push_back(topFace);
        
        // Bottom face (-Y normal)  
        Face bottomFace; bottomFace.materialId = mat; bottomFace.tint = tint;
        bottomFace.vertices = {{minX, minY, minZ}, {maxX, minY, minZ}, {maxX, minY, maxZ}, {minX, minY, maxZ}};
        bottomFace.RecalculateNormal(); mapData.faces.push_back(bottomFace);
        
        // Front face (-Z normal) - visible from positive Z
        Face frontFace; frontFace.materialId = mat; frontFace.tint = tint;
        frontFace.vertices = {{minX, minY, minZ}, {minX, maxY, minZ}, {maxX, maxY, minZ}, {maxX, minY, minZ}};
        frontFace.RecalculateNormal();
        LOG_INFO("STAIRCASE FACE: front face normal (" + std::to_string(frontFace.normal.x) + "," +
                 std::to_string(frontFace.normal.y) + "," + std::to_string(frontFace.normal.z) + 
                 ") material ID " + std::to_string(frontFace.materialId));
        mapData.faces.push_back(frontFace);
        
        // Back face (+Z normal)
        Face backFace; backFace.materialId = mat; backFace.tint = tint;
        backFace.vertices = {{maxX, minY, maxZ}, {maxX, maxY, maxZ}, {minX, maxY, maxZ}, {minX, minY, maxZ}};
        backFace.RecalculateNormal(); mapData.faces.push_back(backFace);
        
        // Left face (-X normal)
        Face leftFace; leftFace.materialId = mat; leftFace.tint = tint;
        leftFace.vertices = {{minX, minY, maxZ}, {minX, maxY, maxZ}, {minX, maxY, minZ}, {minX, minY, minZ}};
        leftFace.RecalculateNormal(); mapData.faces.push_back(leftFace);
        
        // Right face (+X normal)
        Face rightFace; rightFace.materialId = mat; rightFace.tint = tint;
        rightFace.vertices = {{maxX, minY, minZ}, {maxX, maxY, minZ}, {maxX, maxY, maxZ}, {maxX, minY, maxZ}};
        rightFace.RecalculateNormal(); mapData.faces.push_back(rightFace);
    };

    // Room 1 Floor (horizontal face) material 1 (light)
    AddHorizontalFace(Vector3{-5.0f, floorY, -5.0f}, Vector3{5.0f, floorY, 5.0f}, floorY, 1, WHITE);
    
    // Room 1 North wall (vertical surface) material 0 (wall)
    AddVerticalWall(Vector3{-5.0f, floorY, -5.0f}, Vector3{5.0f, floorY, -5.0f}, wallHeight, 0, WHITE);
    
    // Room 1 South wall (vertical surface)
    AddVerticalWall(Vector3{5.0f, floorY, 5.0f}, Vector3{-5.0f, floorY, 5.0f}, wallHeight, 0, WHITE);
    
    // Room 1 West wall (vertical surface) - split to create corridor opening
    // South part: Z=5 to Z=2
    AddVerticalWall(Vector3{-5.0f, floorY, 5.0f}, Vector3{-5.0f, floorY, 2.0f}, wallHeight, 0, WHITE);
    // North part: Z=-2 to Z=-5
    AddVerticalWall(Vector3{-5.0f, floorY, -2.0f}, Vector3{-5.0f, floorY, -5.0f}, wallHeight, 0, WHITE);
    
    // Room 1 East wall (with opening for corridor from Z=-2 to Z=2)
    AddVerticalWall(Vector3{5.0f, floorY, -5.0f}, Vector3{5.0f, floorY, -2.0f}, wallHeight, 0, WHITE);
    AddVerticalWall(Vector3{5.0f, floorY, 2.0f}, Vector3{5.0f, floorY, 5.0f}, wallHeight, 0, WHITE);

    // === CORRIDOR: X=5 to X=15, Z=-2 to Z=2, HAS CEILING ===
    
    // Corridor Floor (material 1)
    AddHorizontalFace(Vector3{5.0f, floorY, -2.0f}, Vector3{15.0f, floorY, 2.0f}, floorY, 1, WHITE);
    
    // Corridor North wall (swap start/end to flip normal inward)
    AddVerticalWall(Vector3{15.0f, floorY, 2.0f}, Vector3{5.0f, floorY, 2.0f}, wallHeight, 0, WHITE);
    
    // Corridor South wall (swap start/end to flip normal inward)
    AddVerticalWall(Vector3{5.0f, floorY, -2.0f}, Vector3{15.0f, floorY, -2.0f}, wallHeight, 0, WHITE);
    
    // Corridor ceiling as horizontal face at ceilingY (material 2)
    AddHorizontalFace(Vector3{5.0f, ceilingY, -2.0f}, Vector3{15.0f, ceilingY, 2.0f}, ceilingY, 2, WHITE);

    // === ROOM 2 (Destination): X=15 to X=27, Z=-6 to Z=6, HAS CEILING ===
    
    // Room 2 Floor (material 1)
    AddHorizontalFace(Vector3{15.0f, floorY, -6.0f}, Vector3{27.0f, floorY, 6.0f}, floorY, 1, WHITE);
    
    // Room 2 North wall
    AddVerticalWall(Vector3{15.0f, floorY, -6.0f}, Vector3{27.0f, floorY, -6.0f}, wallHeight, 0, WHITE);
    
    // Room 2 South wall
    AddVerticalWall(Vector3{27.0f, floorY, 6.0f}, Vector3{15.0f, floorY, 6.0f}, wallHeight, 0, WHITE);
    
    // Room 2 East wall (with opening for corridor to Room 3 from Z=-2 to Z=2)
    AddVerticalWall(Vector3{27.0f, floorY, -6.0f}, Vector3{27.0f, floorY, -2.0f}, wallHeight, 0, WHITE);
    AddVerticalWall(Vector3{27.0f, floorY, 2.0f}, Vector3{27.0f, floorY, 6.0f}, wallHeight, 0, WHITE);
    
    // Room 2 West wall (with opening for corridor from Z=-2 to Z=2)
    AddVerticalWall(Vector3{15.0f, floorY, 6.0f}, Vector3{15.0f, floorY, 2.0f}, wallHeight, 0, WHITE);
    AddVerticalWall(Vector3{15.0f, floorY, -2.0f}, Vector3{15.0f, floorY, -6.0f}, wallHeight, 0, WHITE);
    
    // Room 2 ceiling (material 2)
    AddHorizontalFace(Vector3{15.0f, ceilingY, -6.0f}, Vector3{27.0f, ceilingY, 6.0f}, ceilingY, 2, WHITE);


    // === CORRIDOR 2: Room 2 to Room 3: X=27 to X=37, Z=-2 to Z=2, HAS CEILING ===
    
    // Corridor 2 Floor (material 1)
    AddHorizontalFace(Vector3{27.0f, floorY, -2.0f}, Vector3{37.0f, floorY, 2.0f}, floorY, 1, WHITE);
    
    // Corridor 2 North wall
    AddVerticalWall(Vector3{37.0f, floorY, 2.0f}, Vector3{27.0f, floorY, 2.0f}, wallHeight, 0, WHITE);
    
    // Corridor 2 South wall  
    AddVerticalWall(Vector3{27.0f, floorY, -2.0f}, Vector3{37.0f, floorY, -2.0f}, wallHeight, 0, WHITE);
    
    // Corridor 2 ceiling (material 2)
    AddHorizontalFace(Vector3{27.0f, ceilingY, -2.0f}, Vector3{37.0f, ceilingY, 2.0f}, ceilingY, 2, WHITE);

    // === NEW: ENHANCED TESTING FEATURES ===
    
    // === ROOM 3 (Large Testing Area): X=37 to X=55, Z=-12 to Z=12 ===
    
    // Room 3 Floor (large open area for testing)
    AddHorizontalFace(Vector3{37.0f, floorY, -12.0f}, Vector3{55.0f, floorY, 12.0f}, floorY, 1, WHITE);
    
    // Room 3 walls (opening in west wall connects to Corridor 2)
    AddVerticalWall(Vector3{37.0f, floorY, -12.0f}, Vector3{55.0f, floorY, -12.0f}, wallHeight, 0, WHITE); // North
    AddVerticalWall(Vector3{55.0f, floorY, 12.0f}, Vector3{37.0f, floorY, 12.0f}, wallHeight, 0, WHITE);   // South
    AddVerticalWall(Vector3{55.0f, floorY, -12.0f}, Vector3{55.0f, floorY, 12.0f}, wallHeight, 0, WHITE);  // East
    
    // Room 3 West wall with opening (door from Corridor 2, Z=-2 to Z=2)
    AddVerticalWall(Vector3{37.0f, floorY, 12.0f}, Vector3{37.0f, floorY, 2.0f}, wallHeight, 0, WHITE);
    AddVerticalWall(Vector3{37.0f, floorY, -2.0f}, Vector3{37.0f, floorY, -12.0f}, wallHeight, 0, WHITE);
    
    // Room 3 ceiling REMOVED - open like Room 1

    // === TESTING PLATFORMS (Various heights for jump testing) ===
    
    const float platform1Y = 1.0f;  // Low platform - should step up automatically
    const float platform2Y = 2.0f;  // Medium platform - requires jumping
    const float platform3Y = 3.5f;  // High platform - requires jumping
    
    // Platform 1 (low step-up test) - solid box from X=40-43, Z=8-11, Y=0-1
    AddSolidBox(Vector3{40.0f, floorY, 8.0f}, Vector3{43.0f, platform1Y, 11.0f}, 0, WHITE);

    // Platform 2 (medium jump test) - solid box from X=46-49, Z=8-11, Y=0-2
    AddSolidBox(Vector3{46.0f, floorY, 8.0f}, Vector3{49.0f, platform2Y, 11.0f}, 0, WHITE);

    // Platform 3 (high jump test) - solid box from X=52-55, Z=8-11, Y=0-3.5
    AddSolidBox(Vector3{52.0f, floorY, 8.0f}, Vector3{55.0f, platform3Y, 11.0f}, 0, WHITE);

    // === SLOPE TESTING AREA ===
    
    // Create stepped platforms to simulate slope climbing (X=40-52, Z=-11 to Z=-8)
    const int slopeSteps = 6;
    const float slopeStartX = 40.0f;
    const float slopeEndX = 52.0f;
    const float slopeStartZ = -11.0f;
    const float slopeEndZ = -8.0f;
    const float slopeStartY = 0.0f;
    const float slopeEndY = 3.0f;
    
    for (int i = 0; i < slopeSteps; i++) {
        float t = (float)i / (float)(slopeSteps - 1);
        float nextT = (float)(i + 1) / (float)(slopeSteps - 1);
        
        float x1 = slopeStartX + t * (slopeEndX - slopeStartX);
        float x2 = slopeStartX + nextT * (slopeEndX - slopeStartX);
        float stepTop = slopeStartY + nextT * (slopeEndY - slopeStartY);
        
        // Create solid box for each slope step (full height from floor to step top)
        AddSolidBox(Vector3{x1, floorY, slopeStartZ},
                    Vector3{x2, stepTop, slopeEndZ},
                    0, WHITE);
    }
    
    // === SMOOTH SLOPE TESTING AREA ===
    
    // Create a true smooth slope using angled faces (X=40-52, Z=5 to Z=8)
    const float smoothSlopeStartX = 40.0f;
    const float smoothSlopeEndX = 52.0f;
    const float smoothSlopeStartZ = 5.0f;
    const float smoothSlopeEndZ = 8.0f;
    const float smoothSlopeStartY = 0.0f;
    const float smoothSlopeEndY = 2.5f;
    
    // Create SOLID slope with proper geometry (top surface + back wall + side walls)

    // TOP SURFACE - Two triangles forming the slope
    Face slopeFace1;
    slopeFace1.materialId = 1; // Use material ID 1 (light texture) - try different material
    slopeFace1.tint = WHITE;
    slopeFace1.vertices = {
        {smoothSlopeStartX, smoothSlopeStartY, smoothSlopeStartZ}, // Bottom left
        {smoothSlopeStartX, smoothSlopeStartY, smoothSlopeEndZ},   // Bottom right
        {smoothSlopeEndX, smoothSlopeEndY, smoothSlopeEndZ}        // Top right
    };
    slopeFace1.RecalculateNormal();
    LOG_INFO("SLOPE TOP FACE 1: Created with materialId=" + std::to_string(slopeFace1.materialId) +
             ", normal (" + std::to_string(slopeFace1.normal.x) + "," +
             std::to_string(slopeFace1.normal.y) + "," + std::to_string(slopeFace1.normal.z) + ")");
    mapData.faces.push_back(slopeFace1);

    Face slopeFace2;
    slopeFace2.materialId = 1; // Use material ID 1 (light texture) - try different material
    slopeFace2.tint = WHITE;
    slopeFace2.vertices = {
        {smoothSlopeStartX, smoothSlopeStartY, smoothSlopeStartZ}, // Bottom left
        {smoothSlopeEndX, smoothSlopeEndY, smoothSlopeEndZ},       // Top right
        {smoothSlopeEndX, smoothSlopeEndY, smoothSlopeStartZ}      // Top left
    };
    slopeFace2.RecalculateNormal();
    LOG_INFO("SLOPE TOP FACE 2: Created with normal (" + std::to_string(slopeFace2.normal.x) + "," +
             std::to_string(slopeFace2.normal.y) + "," + std::to_string(slopeFace2.normal.z) + ")");
    mapData.faces.push_back(slopeFace2);

    // BACK WALL - Solid wall at the end of the slope
    Face backWall1;
    backWall1.materialId = 0; // Use material ID 0 (dark texture)
    backWall1.tint = WHITE;
    backWall1.vertices = {
        {smoothSlopeEndX, smoothSlopeEndY, smoothSlopeStartZ},     // Top left
        {smoothSlopeEndX, smoothSlopeEndY, smoothSlopeEndZ},       // Top right
        {smoothSlopeEndX, smoothSlopeStartY, smoothSlopeEndZ}      // Bottom right
    };
    backWall1.RecalculateNormal();
    mapData.faces.push_back(backWall1);

    Face backWall2;
    backWall2.materialId = 0; // Use material ID 0 (dark texture)
    backWall2.tint = WHITE;
    backWall2.vertices = {
        {smoothSlopeEndX, smoothSlopeEndY, smoothSlopeStartZ},     // Top left
        {smoothSlopeEndX, smoothSlopeStartY, smoothSlopeEndZ},     // Bottom right
        {smoothSlopeEndX, smoothSlopeStartY, smoothSlopeStartZ}    // Bottom left
    };
    backWall2.RecalculateNormal();
    mapData.faces.push_back(backWall2);


    // === STAIRS TEST AREA ===
    
    // Create proper solid stairs in the corner (X=42-47, Z=-5 to Z=0)
    const int numStairs = 5;
    const float stairHeight = 0.4f; // Each step is 0.4 units high (within step-up range)
    const float stairDepth = 1.0f;
    
    for (int i = 0; i < numStairs; i++) {
        float stairY = floorY + (i + 1) * stairHeight; // Top of this step
        float stairZ = -5.0f + i * stairDepth;          // Front edge of this step (original position)

        // Create solid box for each step (full height from floor to step top)
        LOG_INFO("Creating staircase step " + std::to_string(i) + " at Y=" + std::to_string(stairY) +
                 " Z=[" + std::to_string(stairZ) + "," + std::to_string(stairZ + stairDepth) + "]");
        LOG_INFO("STAIRS DEBUG: Adding stair step " + std::to_string(i) + " with material ID 0 (dark texture)");
        AddSolidBox(Vector3{42.0f, floorY, stairZ},
                    Vector3{47.0f, stairY, stairZ + stairDepth},
                    0, WHITE); // Use material ID 0 (dark texture) with WHITE tint
    }

    // === CORRIDOR 2: Connecting Room 1 to Room 3 (X=-5 to X=-10, Z=-2 to Z=2) ===

    // North wall (constant Z=-2, spans X=-10 to X=-5)
    AddVerticalWall(Vector3{-10.0f, floorY, -2.0f}, Vector3{-5.0f, floorY, -2.0f}, wallHeight, 0, WHITE);
    // South wall (constant Z=2, spans X=-5 to X=-10)
    AddVerticalWall(Vector3{-5.0f, floorY, 2.0f}, Vector3{-10.0f, floorY, 2.0f}, wallHeight, 0, WHITE);
    // Floor
    AddHorizontalFace(Vector3{-10.0f, floorY, -2.0f}, Vector3{-5.0f, floorY, 2.0f}, floorY, 1, WHITE);
    // Ceiling
    AddHorizontalFace(Vector3{-10.0f, ceilingY, -2.0f}, Vector3{-5.0f, ceilingY, 2.0f}, ceilingY, 2, WHITE);

    // === ROOM 3 (Beyond Corridor 2): -20 to -10 in X, -5 to 5 in Z, NO CEILING ===

    // North wall
    AddVerticalWall(Vector3{-20.0f, floorY, -5.0f}, Vector3{-10.0f, floorY, -5.0f}, wallHeight, 0, WHITE);
    // South wall
    AddVerticalWall(Vector3{-10.0f, floorY, 5.0f}, Vector3{-20.0f, floorY, 5.0f}, wallHeight, 0, WHITE);
    // East wall (with corridor opening from Z=-2 to Z=2)
    AddVerticalWall(Vector3{-10.0f, floorY, -5.0f}, Vector3{-10.0f, floorY, -2.0f}, wallHeight, 0, WHITE);
    AddVerticalWall(Vector3{-10.0f, floorY, 2.0f}, Vector3{-10.0f, floorY, 5.0f}, wallHeight, 0, WHITE);
    // West wall
    AddVerticalWall(Vector3{-20.0f, floorY, 5.0f}, Vector3{-20.0f, floorY, -5.0f}, wallHeight, 0, WHITE);
    // Floor
    AddHorizontalFace(Vector3{-20.0f, floorY, -5.0f}, Vector3{-10.0f, floorY, 5.0f}, floorY, 1, WHITE);
    // NO CEILING - open to sky!

    LOG_INFO("Enhanced test map now includes platforms, slopes, stairs, and Room 3 for comprehensive testing");
}

void WorldSystem::CreateRoomGeometry(MapData& mapData) {
    LOG_INFO("Creating first room geometry");
    // No-op: face-based geometry is generated in AddTestGeometry()
}

void WorldSystem::CreateCorridorGeometry(MapData& mapData) {
    LOG_INFO("Creating corridor geometry");
    // No-op: face-based geometry is generated in AddTestGeometry()
}

void WorldSystem::CreateSecondRoomGeometry(MapData& mapData) {
    LOG_INFO("Creating second room geometry");
    // No-op: face-based geometry is generated in AddTestGeometry()
}

// REMOVED: CreateSurfaceEntity - ARCHITECTURAL FIX
// Static surfaces are no longer converted to entities!
// They are stored in WorldGeometry and rendered by WorldRenderer.

// NEW: Add a rotating pyramid in Room 3 to test Mesh component
void WorldSystem::AddTestDynamicEntity() {
    // Add orange cube entity in Room 2 (static, non-rotating test)
    LOG_INFO("Adding orange cube entity in Room 2");

    Entity* cubeEntity = GetEngine()->CreateEntity();
    LOG_INFO("Created cube entity with ID: " + std::to_string(cubeEntity->GetId()));

    // Add position component in Room 2 (center-ish)
    cubeEntity->AddComponent<Position>(21.0f, 2.0f, 0.0f); // Floating above ground
    LOG_INFO("Added Position component to cube at (21, 2, 0) in Room 2");

    // Add transform component for the cube (static - no rotation)
    auto cubeTransform = cubeEntity->AddComponent<TransformComponent>();
    cubeTransform->position = {21.0f, 2.0f, 0.0f};
    cubeTransform->rotation = QuaternionIdentity(); // No rotation
    LOG_INFO("Added Transform component to cube (static)");

    // Add mesh component with cube geometry
    auto cubeMesh = cubeEntity->AddComponent<MeshComponent>();

    // Use MeshSystem to create cube geometry
    auto meshSystem = GetEngine()->GetSystem<MeshSystem>();
    if (meshSystem) {
        meshSystem->CreateCube(cubeEntity, 2.0f, WHITE);
        meshSystem->SetMaterial(cubeEntity, 3); // Use orange texture material ID 3

        // Set the actual texture for rendering
        if (worldGeometry_ && worldGeometry_->materials.size() > 3) {
            meshSystem->SetTexture(cubeEntity, worldGeometry_->materials[3].texture);
            LOG_INFO("Set orange texture on cube mesh (texture ID: " + std::to_string(worldGeometry_->materials[3].texture.id) + ")");
        } else {
            LOG_WARNING("Could not set texture on cube - worldGeometry or material 3 not available");
        }

        LOG_INFO("Added Mesh component with orange cube (material ID 3)");
    } else {
        LOG_ERROR("MeshSystem not available for cube creation");
    }

    // Add velocity component (stationary)
    auto* cubeVelocity = cubeEntity->AddComponent<Velocity>();
    cubeVelocity->SetVelocity({0.0f, 0.0f, 0.0f});
    LOG_INFO("Added Velocity component (stationary)");

    // Add collision component for collision testing
    auto* cubeCollidable = cubeEntity->AddComponent<Collidable>(Vector3{2.0f, 2.0f, 2.0f}); // Cube bounding box
    cubeCollidable->SetCollisionLayer(LAYER_DEBRIS);
    cubeCollidable->SetCollisionMask(LAYER_WORLD | LAYER_PLAYER | LAYER_DEBRIS);
    LOG_INFO("Added Collidable component for cube collision testing");

    // Register entity with systems AFTER components are added
    LOG_INFO("Registering cube entity with systems (after components added)");
    GetEngine()->UpdateEntityRegistration(cubeEntity);

    // Add to dynamic entities list
    dynamicEntities_.push_back(cubeEntity);
    LOG_INFO("Orange cube entity added (static mesh test, rendered by RenderSystem)");

    // Now add rotating pyramid entity in Room 3
    LOG_INFO("Adding rotating pyramid entity in Room 3");

    Entity* pyramidEntity = GetEngine()->CreateEntity();
    LOG_INFO("Created pyramid entity with ID: " + std::to_string(pyramidEntity->GetId()));

    // Add position component hovering in Room 3 (center of Room 3)
    pyramidEntity->AddComponent<Position>(-15.0f, 3.0f, 0.0f); // Hovering above ground
    LOG_INFO("Added Position component to pyramid at (-15, 3, 0) in Room 3");

    // Add transform component for the pyramid (will be rotated)
    auto pyramidTransform = pyramidEntity->AddComponent<TransformComponent>();
    pyramidTransform->position = {-15.0f, 3.0f, 0.0f};
    pyramidTransform->rotation = QuaternionIdentity(); // Start with no rotation
    LOG_INFO("Added Transform component to pyramid (rotating)");

    // Add mesh component with pyramid geometry
    auto pyramidMesh = pyramidEntity->AddComponent<MeshComponent>();

    // Use MeshSystem to create pyramid geometry
    auto pyramidMeshSystem = GetEngine()->GetSystem<MeshSystem>();
    if (pyramidMeshSystem) {
        pyramidMeshSystem->CreatePyramid(pyramidEntity, 2.0f, 3.0f, {RED, GREEN, BLUE, YELLOW, GRAY}); // Different colored faces
        LOG_INFO("Added Mesh component with colored pyramid");
    } else {
        LOG_ERROR("MeshSystem not available for pyramid creation");
    }

    // Add velocity component for movement (optional)
    auto* pyramidVelocity = pyramidEntity->AddComponent<Velocity>();
    pyramidVelocity->SetVelocity({0.0f, 0.0f, 0.0f}); // No movement for now
    LOG_INFO("Added Velocity component (stationary for now)");

    // Add collision component for collision testing
    auto* pyramidCollidable = pyramidEntity->AddComponent<Collidable>(Vector3{2.0f, 3.0f, 2.0f}); // Pyramid bounding box
    pyramidCollidable->SetCollisionLayer(LAYER_DEBRIS);
    pyramidCollidable->SetCollisionMask(LAYER_WORLD | LAYER_PLAYER | LAYER_DEBRIS);
    LOG_INFO("Added Collidable component for pyramid collision testing");

    // Register entity with systems AFTER components are added
    LOG_INFO("Registering pyramid entity with systems (after components added)");
    GetEngine()->UpdateEntityRegistration(pyramidEntity);

    // Add to dynamic entities list
    dynamicEntities_.push_back(pyramidEntity);
    LOG_INFO("Rotating pyramid entity added (rendered by RenderSystem)");
}

Entity* WorldSystem::CreateLightEntity(const Vector3& position, const Color& color, float intensity) {
    Entity* entity = GetEngine()->CreateEntity();

    entity->AddComponent<Position>(position);

    // Add light component (would be implemented in lighting system)
    // auto light = entity->AddComponent<Light>();
    // light->SetColor(color);
    // light->SetIntensity(intensity);

    dynamicEntities_.push_back(entity);
    return entity;
}

// REMOVED: Old BSP building methods - now handled by WorldGeometry::BuildWorldGeometry()
