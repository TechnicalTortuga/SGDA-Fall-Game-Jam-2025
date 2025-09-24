#include "MeshSystem.h"
#include "../../utils/Logger.h"
#include <cmath>
#include "../Components/TransformComponent.h"
#include "../Components/MaterialComponent.h"
#include "../Components/TextureComponent.h"
#include "../Systems/AssetSystem.h"
#include "../Systems/RenderSystem.h"
#include "raylib.h"
#include "raymath.h"

MeshSystem::MeshSystem()
    : initialized_(false), nextCompositeMeshId_(1) {
    LOG_INFO("MeshSystem created");
}

MeshSystem::~MeshSystem() {
    LOG_INFO("MeshSystem destroyed");
}

void MeshSystem::Initialize() {
    if (initialized_) {
        LOG_WARNING("MeshSystem already initialized");
        return;
    }

    LOG_INFO("Initializing MeshSystem");

    // Set up component signature for MeshComponent
    SetSignature<MeshComponent>();

    initialized_ = true;
    LOG_INFO("MeshSystem initialized successfully");
}

void MeshSystem::Update(float deltaTime) {
    // Mesh system doesn't need per-frame updates for now
    // Could be used for mesh animations or procedural generation later
}

void MeshSystem::Shutdown() {
    if (!initialized_) return;

    LOG_INFO("Shutting down MeshSystem");
    
    // Clear mesh cache on shutdown
    // ClearMeshCache removed - handled by CacheSystem automatically
    
    initialized_ = false;
}

// Entity-based mesh operations
void MeshSystem::CreateCube(Entity* entity, float size, const Color& color) {
    if (!entity) {
        LOG_ERROR("MeshSystem::CreateCube - Invalid entity");
        return;
    }

    auto* mesh = GetMeshComponent(entity);
    if (!mesh) {
        LOG_ERROR("MeshSystem::CreateCube - Entity has no MeshComponent");
        return;
    }

    CreateCube(*mesh, size, color);
    mesh->needsRebuild = true;
    InvalidateEntityCache(entity);
    LOG_DEBUG("Created cube mesh for entity " + std::to_string(entity->GetId()));
}

void MeshSystem::CreatePyramid(Entity* entity, float baseSize, float height,
                              const std::vector<Color>& faceColors) {
    if (!entity) {
        LOG_ERROR("MeshSystem::CreatePyramid - Invalid entity");
        return;
    }

    auto* mesh = GetMeshComponent(entity);
    if (!mesh) {
        LOG_ERROR("MeshSystem::CreatePyramid - Entity has no MeshComponent");
        return;
    }

    // Check MaterialComponent for gradient mode and generate appropriate colors
    auto* material = entity->GetComponent<MaterialComponent>();
    std::vector<Color> pyramidColors = faceColors; // Default fallback

    if (material) {
        // Get MaterialSystem from engine
        auto materialSystem = engine_.GetSystem<MaterialSystem>();
        if (materialSystem) {
            if (material->IsLinearGradient()) {
                // Generate linear gradient colors
                Color primary = material->GetPrimaryColor(materialSystem);
                Color secondary = material->GetSecondaryColor(materialSystem);
                pyramidColors = GenerateLinearGradientColors(primary, secondary, 4); // 4 faces
                LOG_DEBUG("Created linear gradient pyramid for entity " + std::to_string(entity->GetId()));
            } else if (material->IsRadialGradient()) {
                // Generate radial gradient colors
                Color primary = material->GetPrimaryColor(materialSystem);
                Color secondary = material->GetSecondaryColor(materialSystem);
                pyramidColors = GenerateRadialGradientColors(primary, secondary, 4); // 4 faces
                LOG_DEBUG("Created radial gradient pyramid for entity " + std::to_string(entity->GetId()));
            } else {
                // Solid color (GRADIENT_NONE)
                Color primary = material->GetPrimaryColor(materialSystem);
                pyramidColors = {primary, primary, primary, primary}; // Same color for all faces
                LOG_DEBUG("Created solid color pyramid for entity " + std::to_string(entity->GetId()));
            }
        }
    } else {
        LOG_DEBUG("Created pyramid with default colors for entity " + std::to_string(entity->GetId()));
    }

    CreatePyramid(*mesh, baseSize, height, pyramidColors);

    mesh->needsRebuild = true;
    InvalidateEntityCache(entity);
}

void MeshSystem::CreateCustomMesh(Entity* entity, const std::vector<MeshVertex>& vertices,
                                 const std::vector<MeshTriangle>& triangles) {
    if (!entity) {
        LOG_ERROR("MeshSystem::CreateCustomMesh - Invalid entity");
        return;
    }

    auto* mesh = GetMeshComponent(entity);
    if (!mesh) {
        LOG_ERROR("MeshSystem::CreateCustomMesh - Entity has no MeshComponent");
        return;
    }

    mesh->vertices = vertices;
    mesh->triangles = triangles;
    mesh->needsRebuild = true;
    InvalidateEntityCache(entity);
    LOG_DEBUG("Created custom mesh for entity " + std::to_string(entity->GetId()) +
              " with " + std::to_string(vertices.size()) + " vertices, " +
              std::to_string(triangles.size()) + " triangles");
}

void MeshSystem::ClearMesh(Entity* entity) {
    if (!entity) return;

    auto* mesh = GetMeshComponent(entity);
    if (!mesh) return;

    ClearMesh(*mesh);
    InvalidateEntityCache(entity);
}

void MeshSystem::AddVertex(Entity* entity, const Vector3& position, const Vector3& normal,
                           const Vector2& texCoord, const Color& color) {
    if (!entity) return;

    auto* mesh = GetMeshComponent(entity);
    if (!mesh) return;

    AddVertex(*mesh, position, normal, texCoord, color);
}

void MeshSystem::AddTriangle(Entity* entity, unsigned int v1, unsigned int v2, unsigned int v3) {
    if (!entity) return;

    auto* mesh = GetMeshComponent(entity);
    if (!mesh) return;

    AddTriangle(*mesh, v1, v2, v3);
}

void MeshSystem::AddQuad(Entity* entity, unsigned int v1, unsigned int v2, unsigned int v3, unsigned int v4) {
    if (!entity) return;

    auto* mesh = GetMeshComponent(entity);
    if (!mesh) return;

    AddQuad(*mesh, v1, v2, v3, v4);
}

// Modern primitive methods (use Raylib's built-in mesh generation)
void MeshSystem::CreateCube(MeshComponent& mesh, float size, const Color& color) {
    // Clear any existing mesh data
    ClearMesh(mesh);
    
    // Set as primitive type - Raylib will generate the actual mesh
    mesh.meshType = MeshComponent::MeshType::PRIMITIVE;
    mesh.primitiveShape = "cube";
    mesh.meshName = "cube_" + std::to_string(size);
    
    // Store primitive parameters for Raylib generation
    // No need to generate vertices/triangles - RenderAssetCache will use GenMeshCube()
    
    LOG_INFO("Set up cube primitive (size: " + std::to_string(size) + ") - Raylib will generate geometry");
}

void MeshSystem::CreatePyramid(MeshComponent& mesh, float baseSize, float height,
                              const std::vector<Color>& faceColors) {
    mesh.meshType = MeshComponent::MeshType::MODEL;
    mesh.primitiveShape = "pyramid";
    mesh.meshName = "pyramid_" + std::to_string(baseSize) + "x" + std::to_string(height);

    mesh.vertices.clear();
    mesh.triangles.clear();

    CreatePyramidGeometry(mesh.vertices, mesh.triangles, baseSize, height, faceColors);

    LOG_INFO("Created custom pyramid mesh (base radius: " + std::to_string(baseSize) +
             ", height: " + std::to_string(height) + ")");
}

void MeshSystem::CreateSphere(Entity* entity, float radius) {
    MeshComponent* meshComp = GetMeshComponent(entity);
    if (!meshComp) {
        LOG_ERROR("MeshSystem::CreateSphere - Entity has no MeshComponent");
        return;
    }
    
    // Clear any existing mesh data
    ClearMesh(*meshComp);
    
    // Set as primitive type - Raylib will generate the actual mesh when needed
    meshComp->meshType = MeshComponent::MeshType::PRIMITIVE;
    meshComp->primitiveShape = "sphere";
    meshComp->meshName = "sphere_" + std::to_string(radius);
    
    // No need to generate vertices/triangles - RenderAssetCache will use GenMeshSphere()
    
    LOG_INFO("Set up sphere primitive (radius: " + std::to_string(radius) + ") - Raylib will generate geometry when rendering");
}

void MeshSystem::CreateCapsule(Entity* entity, float radius, float height) {
    MeshComponent* meshComp = GetMeshComponent(entity);
    if (!meshComp) {
        LOG_ERROR("MeshSystem::CreateCapsule - Entity has no MeshComponent");
        return;
    }
    ClearMesh(*meshComp);

    float minHeight = radius * 2.0f + 0.001f;
    if (height < minHeight) {
        LOG_WARNING("CreateCapsule: height " + std::to_string(height) +
                    " too small for radius " + std::to_string(radius) + ", clamping to minimum");
        height = minHeight;
    }

    meshComp->meshType = MeshComponent::MeshType::MODEL;
    meshComp->primitiveShape = "capsule";
    meshComp->meshName = "capsule_" + std::to_string(radius) + "x" + std::to_string(height);

    // Standard capsule formula: cylinderHeight = totalHeight - 2*radius
    const float cylinderHeight = std::max(height - (2.0f * radius), 0.0f);
    
    LOG_INFO("Capsule construction: totalHeight=" + std::to_string(height) + 
             ", radius=" + std::to_string(radius) + 
             ", cylinderHeight=" + std::to_string(cylinderHeight));

    Mesh cylinderMesh = {0};
    Mesh hemiMesh = {0};

    if (cylinderHeight > 0.0001f) {
        cylinderMesh = GenMeshCylinder(radius, cylinderHeight, 24);
    }

    hemiMesh = GenMeshHemiSphere(radius, 16, 32);

    auto appendMesh = [&](const Mesh& source, auto positionTransform, auto normalTransform, bool invertWinding = false) {
        if (source.vertexCount <= 0 || !source.vertices) {
            return;
        }

        unsigned int baseIndex = static_cast<unsigned int>(meshComp->vertices.size());

        for (int i = 0; i < source.vertexCount; ++i) {
            Vector3 pos = {source.vertices[i * 3], source.vertices[i * 3 + 1], source.vertices[i * 3 + 2]};
            pos = positionTransform(pos);

            Vector3 normal = {0.0f, 1.0f, 0.0f};
            if (source.normals) {
                normal = {source.normals[i * 3], source.normals[i * 3 + 1], source.normals[i * 3 + 2]};
                normal = normalTransform(normal);
                normal = Vector3Normalize(normal);
            }

            Vector2 texCoord = {0.0f, 0.0f};
            if (source.texcoords) {
                texCoord = {source.texcoords[i * 2], source.texcoords[i * 2 + 1]};
            }

            meshComp->vertices.push_back({pos, normal, texCoord, WHITE});
        }

        if (source.triangleCount <= 0) {
            return;
        }

        if (source.indices) {
            for (int t = 0; t < source.triangleCount; ++t) {
                unsigned int i0 = baseIndex + source.indices[t * 3];
                unsigned int i1 = baseIndex + source.indices[t * 3 + 1];
                unsigned int i2 = baseIndex + source.indices[t * 3 + 2];

                if (invertWinding) {
                    meshComp->triangles.push_back({i0, i2, i1});
                } else {
                    meshComp->triangles.push_back({i0, i1, i2});
                }
            }
        } else {
            // Assume non-indexed triangle list
            for (int t = 0; t < source.vertexCount / 3; ++t) {
                unsigned int i0 = baseIndex + t * 3;
                unsigned int i1 = baseIndex + t * 3 + 1;
                unsigned int i2 = baseIndex + t * 3 + 2;

                if (invertWinding) {
                    meshComp->triangles.push_back({i0, i2, i1});
                } else {
                    meshComp->triangles.push_back({i0, i1, i2});
                }
            }
        }
    };

    if (hemiMesh.vertices) {
        // Standard capsule positioning:
        // Top hemisphere center: +totalHeight/2 - radius
        // Bottom hemisphere center: -totalHeight/2 + radius
        
        const float topHemiCenter = (height * 0.5f) - radius;
        const float bottomHemiCenter = -(height * 0.5f) + radius;
        
        LOG_INFO("Hemisphere positioning: topCenter=" + std::to_string(topHemiCenter) + 
                 ", bottomCenter=" + std::to_string(bottomHemiCenter));
        
        // Top hemisphere: position its center at the top of the capsule
        appendMesh(hemiMesh,
                   [topHemiCenter](const Vector3& pos) {
                       Vector3 transformed = pos;
                       transformed.y += topHemiCenter;
                       return transformed;
                   },
                   [](const Vector3& normal) { return normal; });

        // Bottom hemisphere: flip and position its center at the bottom of the capsule
        appendMesh(hemiMesh,
                   [bottomHemiCenter](const Vector3& pos) {
                       Vector3 transformed = pos;
                       transformed.y = -pos.y + bottomHemiCenter; // Flip Y and position
                       return transformed;
                   },
                   [](const Vector3& normal) {
                       Vector3 transformed = normal;
                       transformed.y = -transformed.y; // Flip normal
                       return transformed;
                   },
                   true);
    }

    if (cylinderHeight > 0.0001f && cylinderMesh.vertices) {
        appendMesh(cylinderMesh,
                   [radius](const Vector3& pos) { 
                       Vector3 transformed = pos;
                       transformed.y -= radius * 0.5f; // Move cylinder down by half hemisphere height
                       return transformed;
                   },
                   [](const Vector3& normal) { return normal; });
    }

    if (cylinderMesh.vertices) {
        UnloadMesh(cylinderMesh);
    }
    if (hemiMesh.vertices) {
        UnloadMesh(hemiMesh);
    }

    LOG_INFO("Created custom capsule mesh (radius: " + std::to_string(radius) +
             ", height: " + std::to_string(height) + ", vertices: " +
             std::to_string(meshComp->vertices.size()) + ")");
}

void MeshSystem::CreateCylinder(Entity* entity, float radius, float height) {
    MeshComponent* meshComp = GetMeshComponent(entity);
    if (!meshComp) {
        LOG_ERROR("MeshSystem::CreateCylinder - Entity has no MeshComponent");
        return;
    }
    
    // Clear any existing mesh data
    ClearMesh(*meshComp);
    
    // Set as primitive type - Raylib will generate the actual mesh when needed
    meshComp->meshType = MeshComponent::MeshType::PRIMITIVE;
    meshComp->primitiveShape = "cylinder";
    meshComp->meshName = "cylinder_" + std::to_string(radius) + "x" + std::to_string(height);
    
    // No need to generate vertices/triangles - RenderAssetCache will use GenMeshCylinder()
    
    LOG_INFO("Set up cylinder primitive (radius: " + std::to_string(radius) + 
             ", height: " + std::to_string(height) + ") - Raylib will generate geometry when rendering");
}

void MeshSystem::ClearMesh(MeshComponent& mesh) {
    mesh.vertices.clear();
    mesh.triangles.clear();
}

void MeshSystem::AddVertex(MeshComponent& mesh, const Vector3& position, const Vector3& normal,
                           const Vector2& texCoord, const Color& color) {
    mesh.vertices.push_back({position, normal, texCoord, color});
}

void MeshSystem::AddTriangle(MeshComponent& mesh, unsigned int v1, unsigned int v2, unsigned int v3) {
    mesh.triangles.push_back({v1, v2, v3});
}

void MeshSystem::AddQuad(MeshComponent& mesh, unsigned int v1, unsigned int v2, unsigned int v3, unsigned int v4) {
    AddTriangle(mesh, v1, v2, v3);
    AddTriangle(mesh, v1, v3, v4);
}

void MeshSystem::CreatePyramidGeometry(std::vector<MeshVertex>& vertices, std::vector<MeshTriangle>& triangles,
                                      float baseSize, float height, const std::vector<Color>& faceColors) {
    vertices.clear();
    triangles.clear();

    const float half = baseSize * 0.5f;
    const Vector3 apex = {0.0f, height, 0.0f};
    const Vector3 basePositions[4] = {
        {-half, 0.0f, -half},
        { half, 0.0f, -half},
        { half, 0.0f,  half},
        {-half, 0.0f,  half}
    };

    const Vector2 baseUVs[4] = {
        {0.0f, 1.0f},
        {1.0f, 1.0f},
        {1.0f, 0.0f},
        {0.0f, 0.0f}
    };

    auto getFaceColor = [&](size_t index) {
        if (index < faceColors.size()) {
            return faceColors[index];
        }
        return WHITE;
    };

    auto pushVertex = [&](const Vector3& position, const Vector3& normal, const Vector2& uv, const Color& color) {
        vertices.push_back({position, normal, uv, color});
        return static_cast<unsigned int>(vertices.size() - 1);
    };

    auto faceNormal = [](const Vector3& a, const Vector3& b, const Vector3& c) {
        Vector3 ab = Vector3Subtract(b, a);
        Vector3 ac = Vector3Subtract(c, a);
        return Vector3Normalize(Vector3CrossProduct(ab, ac));
    };

    // Create side faces (four triangles)
    for (int face = 0; face < 4; ++face) {
        Vector3 p0 = basePositions[face];
        Vector3 p1 = basePositions[(face + 1) % 4];

        Vector3 normal = faceNormal(apex, p0, p1);

        Vector2 apexUv = {0.5f, 0.0f};
        Vector2 uv0 = baseUVs[face];
        Vector2 uv1 = baseUVs[(face + 1) % 4];

        Color faceColor = getFaceColor(face);

        unsigned int startIndex = static_cast<unsigned int>(vertices.size());
        pushVertex(apex, normal, apexUv, faceColor);
        pushVertex(p0, normal, uv0, faceColor);
        pushVertex(p1, normal, uv1, faceColor);

        triangles.push_back({startIndex, startIndex + 1, startIndex + 2});
    }

    // Base (two triangles forming a quad)
    Color baseColor = faceColors.size() > 4 ? faceColors[4] : WHITE;
    Vector3 baseNormal = {0.0f, -1.0f, 0.0f};

    unsigned int baseStart = static_cast<unsigned int>(vertices.size());
    for (int i = 0; i < 4; ++i) {
        pushVertex(basePositions[i], baseNormal, baseUVs[i], baseColor);
    }

    // Ensure winding order produces downward-pointing normal
    triangles.push_back({baseStart, baseStart + 2, baseStart + 1});
    triangles.push_back({baseStart, baseStart + 3, baseStart + 2});
}

// Utility methods
size_t MeshSystem::GetVertexCount(Entity* entity) const {
    auto* mesh = GetMeshComponent(entity);
    return mesh ? mesh->vertices.size() : 0;
}

size_t MeshSystem::GetTriangleCount(Entity* entity) const {
    auto* mesh = GetMeshComponent(entity);
    return mesh ? mesh->triangles.size() : 0;
}

const std::vector<MeshVertex>* MeshSystem::GetVertices(Entity* entity) const {
    auto* mesh = GetMeshComponent(entity);
    return mesh ? &mesh->vertices : nullptr;
}

const std::vector<MeshTriangle>* MeshSystem::GetTriangles(Entity* entity) const {
    auto* mesh = GetMeshComponent(entity);
    return mesh ? &mesh->triangles : nullptr;
}

// Transform operations (now work with TransformComponent)
float MeshSystem::GetRotationAngle(Entity* entity) const {
    if (!entity) return 0.0f;

    auto* transform = entity->GetComponent<TransformComponent>();
    if (!transform) return 0.0f;

    Vector3 axis;
    float angle;
    QuaternionToAxisAngle(transform->rotation, &axis, &angle);
    return angle;
}

Vector3 MeshSystem::GetRotationAxis(Entity* entity) const {
    if (!entity) return Vector3{0, 1, 0};

    auto* transform = entity->GetComponent<TransformComponent>();
    if (!transform) return Vector3{0, 1, 0};

    Vector3 axis;
    float angle;
    QuaternionToAxisAngle(transform->rotation, &axis, &angle);
    return axis;
}

void MeshSystem::SetRotation(Entity* entity, float angle, const Vector3& axis) {
    if (!entity) return;

    auto* transform = entity->GetComponent<TransformComponent>();
    if (!transform) {
        // Add TransformComponent if it doesn't exist
        entity->AddComponent<TransformComponent>();
        transform = entity->GetComponent<TransformComponent>();
    }

    transform->rotation = QuaternionFromAxisAngle(axis, angle);
}

// Material operations removed - now handled by MaterialSystem
// Materials are managed centrally and MaterialComponents are lightweight handles

// Gradient color generation helpers
std::vector<Color> MeshSystem::GenerateLinearGradientColors(Color primary, Color secondary, int numFaces) {
    std::vector<Color> colors;
    for (int i = 0; i < numFaces; ++i) {
        float t = static_cast<float>(i) / (numFaces - 1); // 0.0 to 1.0
        Color gradientColor = {
            static_cast<unsigned char>((1 - t) * primary.r + t * secondary.r),
            static_cast<unsigned char>((1 - t) * primary.g + t * secondary.g),
            static_cast<unsigned char>((1 - t) * primary.b + t * secondary.b),
            static_cast<unsigned char>((1 - t) * primary.a + t * secondary.a)
        };
        colors.push_back(gradientColor);
    }
    return colors;
}

std::vector<Color> MeshSystem::GenerateRadialGradientColors(Color primary, Color secondary, int numFaces) {
    std::vector<Color> colors;
    for (int i = 0; i < numFaces; ++i) {
        // For radial gradients on pyramid faces, we'll create a gradient from center to edge
        // Using a simple interpolation based on face index
        float t = static_cast<float>(i) / (numFaces - 1); // 0.0 to 1.0
        Color gradientColor = {
            static_cast<unsigned char>((1 - t) * primary.r + t * secondary.r),
            static_cast<unsigned char>((1 - t) * primary.g + t * secondary.g),
            static_cast<unsigned char>((1 - t) * primary.b + t * secondary.b),
            static_cast<unsigned char>((1 - t) * primary.a + t * secondary.a)
        };
        colors.push_back(gradientColor);
    }
    return colors;
}

// Texture operations (now work with TextureComponent and AssetSystem)
Texture2D MeshSystem::GetTexture(Entity* entity) const {
    if (!entity) return Texture2D{0, 0, 0, 0, 0};

    auto* textureComp = entity->GetComponent<TextureComponent>();
    if (!textureComp || !textureComp->isLoaded) return Texture2D{0, 0, 0, 0, 0};

    // Return the texture stored directly in the component
    return textureComp->texture;
}

void MeshSystem::SetTexture(Entity* entity, Texture2D texture) {
    if (!entity) return;

    auto* textureComp = entity->GetComponent<TextureComponent>();
    if (!textureComp) {
        // Add TextureComponent if it doesn't exist
        entity->AddComponent<TextureComponent>();
        textureComp = entity->GetComponent<TextureComponent>();
    }

    // Store the texture directly in the component
    textureComp->texture = texture;
    textureComp->isLoaded = (texture.id != 0);
    textureComp->width = texture.width;
    textureComp->height = texture.height;
    textureComp->mipmaps = texture.mipmaps;
    textureComp->format = texture.format;

    LOG_DEBUG("SetTexture called on entity " + std::to_string(entity->GetId()) +
              " - texture stored (ID: " + std::to_string(texture.id) + ")");
}

void MeshSystem::ResolvePendingTextures() {
    LOG_INFO("ResolvePendingTextures called");

    // Get WorldSystem to access materials
    auto* worldSys = Engine::GetInstance().GetSystem<WorldSystem>();
    if (!worldSys) {
        LOG_ERROR("ResolvePendingTextures: WorldSystem not found");
        return;
    }

    if (!worldSys->GetWorldGeometry()) {
        LOG_ERROR("ResolvePendingTextures: WorldGeometry not available");
        return;
    }

    auto& worldGeometry = *worldSys->GetWorldGeometry();
    LOG_INFO("Found " + std::to_string(worldGeometry.materialIdMap.size()) + " material mappings in world geometry");

    // In the new system, materials are handled through MaterialComponent and MaterialSystem
    // Textures are resolved automatically by AssetSystem when materials are created
    LOG_INFO("Material resolution is now handled automatically through MaterialSystem and AssetSystem");
}

// Legacy geometry creation methods removed - now using direct Raylib primitives

MeshComponent* MeshSystem::GetMeshComponent(Entity* entity) const {
    if (!entity) return nullptr;
    return entity->GetComponent<MeshComponent>();
}

void MeshSystem::InvalidateEntityCache(Entity* entity) const {
    if (!entity) return;
    
    // Get RenderSystem and invalidate the cached model for this entity
    auto* renderSystem = Engine::GetInstance().GetSystem<RenderSystem>();
    if (renderSystem && renderSystem->GetRenderer()) {
        uint64_t entityId = entity->GetId();
        renderSystem->GetRenderer()->InvalidateMeshCache(entityId);
        LOG_DEBUG("Invalidated render cache for entity " + std::to_string(entityId));
    }
}

// Composite mesh management methods
uint64_t MeshSystem::RegisterCompositeMesh(const std::string& name, const std::vector<SubMesh>& subMeshes) {
    uint64_t id = nextCompositeMeshId_++;
    CompositeMeshDefinition definition(name);
    definition.subMeshes = subMeshes;
    
    compositeMeshRegistry_[id] = definition;
    
    LOG_DEBUG("Registered composite mesh '" + name + "' with ID " + std::to_string(id) + 
              " containing " + std::to_string(subMeshes.size()) + " sub-meshes");
    
    return id;
}

const CompositeMeshDefinition* MeshSystem::GetCompositeMeshDefinition(uint64_t compositeMeshId) const {
    auto it = compositeMeshRegistry_.find(compositeMeshId);
    if (it != compositeMeshRegistry_.end()) {
        return &it->second;
    }
    
    LOG_WARNING("Composite mesh definition not found for ID: " + std::to_string(compositeMeshId));
    return nullptr;
}

// Mesh creation and caching methods
// GetOrCreateMesh removed - now handled by CacheSystem
/*Mesh MeshSystem::GetOrCreateMesh(const MeshComponent& meshComponent) {
    // Generate cache key based on mesh properties
    std::string cacheKey;
    
    if (meshComponent.meshType == MeshComponent::MeshType::PRIMITIVE) {
        // For primitives, use shape and basic properties as key
        cacheKey = meshComponent.primitiveShape + "_default";
        
        // Check cache first
        auto it = meshCache_.find(cacheKey);
        if (it != meshCache_.end()) {
            LOG_DEBUG("Mesh cache HIT for key: " + cacheKey);
            return it->second;
        }
        
        // Cache miss - create new primitive mesh
        Mesh newMesh = CreatePrimitiveMesh(meshComponent.primitiveShape);
        if (newMesh.vertexCount > 0) {
            meshCache_[cacheKey] = newMesh;
            LOG_DEBUG("Mesh cache MISS for key: " + cacheKey + " - created and cached new mesh");
            return newMesh;
        } else {
            LOG_ERROR("Failed to create primitive mesh for: " + meshComponent.primitiveShape);
            return Mesh{0};
        }
    }
    else if (meshComponent.meshType == MeshComponent::MeshType::MODEL) {
        // For custom meshes, convert from MeshComponent data
        // This is the legacy path we already had
        if (meshComponent.vertices.empty() || meshComponent.triangles.empty()) {
            LOG_WARNING("Custom mesh has no vertex/triangle data");
            return Mesh{0};
        }
        
        // Convert MeshComponent to Raylib Mesh (no caching for custom meshes for now)
        return ConvertCustomMesh(meshComponent);
    }
    
    LOG_WARNING("Unknown mesh type in GetOrCreateMesh");
    return Mesh{0};
}

Mesh MeshSystem::CreatePrimitiveMesh(const std::string& primitiveType, float size, float radius, float height) const {
    if (primitiveType == "cube") {
        return GenMeshCube(size, size, size);
    }
    else if (primitiveType == "sphere") {
        return GenMeshSphere(radius, 16, 16);
    }
    else if (primitiveType == "cylinder") {
        return GenMeshCylinder(radius, height, 16);
    }
    else {
        LOG_WARNING("Unknown primitive type: " + primitiveType);
        return Mesh{0};
    }
}

Mesh MeshSystem::ConvertCustomMesh(const MeshComponent& meshComponent) const {
    // Convert MeshComponent to Raylib Mesh format (legacy path)
    Mesh raylibMesh = {0};
    raylibMesh.vertexCount = meshComponent.vertices.size();
    raylibMesh.triangleCount = meshComponent.triangles.size();

    // Allocate and copy vertex data
    raylibMesh.vertices = (float*)RL_CALLOC(meshComponent.vertices.size() * 3, sizeof(float));
    raylibMesh.normals = (float*)RL_CALLOC(meshComponent.vertices.size() * 3, sizeof(float));
    raylibMesh.texcoords = (float*)RL_CALLOC(meshComponent.vertices.size() * 2, sizeof(float));
    raylibMesh.colors = (unsigned char*)RL_CALLOC(meshComponent.vertices.size() * 4, sizeof(unsigned char));

    // Allocate and copy index data
    raylibMesh.indices = (unsigned short*)RL_CALLOC(meshComponent.triangles.size() * 3, sizeof(unsigned short));

    // Copy vertex data
    for (size_t i = 0; i < meshComponent.vertices.size(); ++i) {
        const auto& vertex = meshComponent.vertices[i];
        raylibMesh.vertices[i * 3] = vertex.position.x;
        raylibMesh.vertices[i * 3 + 1] = vertex.position.y;
        raylibMesh.vertices[i * 3 + 2] = vertex.position.z;

        raylibMesh.normals[i * 3] = vertex.normal.x;
        raylibMesh.normals[i * 3 + 1] = vertex.normal.y;
        raylibMesh.normals[i * 3 + 2] = vertex.normal.z;

        raylibMesh.texcoords[i * 2] = vertex.texCoord.x;
        raylibMesh.texcoords[i * 2 + 1] = vertex.texCoord.y;

        raylibMesh.colors[i * 4] = vertex.color.r;
        raylibMesh.colors[i * 4 + 1] = vertex.color.g;
        raylibMesh.colors[i * 4 + 2] = vertex.color.b;
        raylibMesh.colors[i * 4 + 3] = vertex.color.a;
    }

    // Copy triangle indices
    for (size_t i = 0; i < meshComponent.triangles.size(); ++i) {
        const auto& triangle = meshComponent.triangles[i];
        raylibMesh.indices[i * 3] = triangle.v1;
        raylibMesh.indices[i * 3 + 1] = triangle.v2;
        raylibMesh.indices[i * 3 + 2] = triangle.v3;
    }

    // Upload mesh to GPU
    UploadMesh(&raylibMesh, false);
    
    return raylibMesh;
}
*/

// ClearMeshCache removed - CacheSystem handles cleanup automatically
