#include "CacheSystem.h"
#include "../Components/MeshComponent.h"
#include "MeshSystem.h"
#include "../../core/Engine.h"
#include <algorithm>

// Model cache factory implementations
ModelCacheKey ModelCacheFactory::GenerateKey(const MeshComponent& mesh) {
    ModelCacheKey key;
    key.meshHash = CalculateMeshHash(mesh);
    return key;
}

std::unique_ptr<CachedModelData> ModelCacheFactory::CreateModelData(const MeshComponent& mesh) {
    auto modelData = std::make_unique<CachedModelData>();
    
    // Get MeshSystem to handle all mesh creation
    auto* meshSystem = Engine::GetInstance().GetSystem<MeshSystem>();
    if (!meshSystem) {
        LOG_ERROR("MeshSystem not available for mesh creation");
        return nullptr;
    }
    
    Mesh raylibMesh = {0};
    
    // 🎯 FIX: Create the correct primitive based on mesh type and shape
    if (mesh.meshType == MeshComponent::MeshType::PRIMITIVE) {
        LOG_INFO("🛠️ GENERATING PRIMITIVE: '" + mesh.primitiveShape + "' (name: " + mesh.meshName + ")");
        
        if (mesh.primitiveShape == "cube") {
            // Extract size from mesh name (format: "cube_1.5")
            float size = 1.0f;
            size_t sizePos = mesh.meshName.find("_");
            if (sizePos != std::string::npos) {
                try {
                    size = std::stof(mesh.meshName.substr(sizePos + 1));
                } catch (...) {
                    LOG_WARNING("Failed to parse size from cube mesh name: " + mesh.meshName);
                }
            }
            raylibMesh = GenMeshCube(size, size, size);
            LOG_INFO("✅ Generated cube mesh (size: " + std::to_string(size) + ")");
        } 
        else if (mesh.primitiveShape == "sphere") {
            // Extract radius from mesh name (format: "sphere_1.5")
            float radius = 1.0f;
            size_t radiusPos = mesh.meshName.find("_");
            if (radiusPos != std::string::npos) {
                try {
                    radius = std::stof(mesh.meshName.substr(radiusPos + 1));
                } catch (...) {
                    LOG_WARNING("Failed to parse radius from sphere mesh name: " + mesh.meshName);
                }
            }
            raylibMesh = GenMeshSphere(radius, 16, 16);
            LOG_INFO("✅ Generated sphere mesh (radius: " + std::to_string(radius) + ")");
        }
        else if (mesh.primitiveShape == "cylinder") {
            // Extract radius and height from mesh name (format: "cylinder_1.2x2.5")
            float radius = 1.0f;
            float height = 2.0f;
            size_t underscorePos = mesh.meshName.find("_");
            if (underscorePos != std::string::npos) {
                size_t xPos = mesh.meshName.find("x", underscorePos);
                if (xPos != std::string::npos) {
                    try {
                        radius = std::stof(mesh.meshName.substr(underscorePos + 1, xPos - underscorePos - 1));
                        height = std::stof(mesh.meshName.substr(xPos + 1));
                    } catch (...) {
                        LOG_WARNING("Failed to parse radius/height from cylinder mesh name: " + mesh.meshName);
                    }
                }
            }
            raylibMesh = GenMeshCylinder(radius, height, 16);
            LOG_INFO("✅ Generated cylinder mesh (radius: " + std::to_string(radius) + ", height: " + std::to_string(height) + ")");
        }
        else if (mesh.primitiveShape == "cone") {
            // Extract radius and height from mesh name (format: "cone_1.2x2.5")
            float radius = 1.0f;
            float height = 2.0f;
            size_t underscorePos = mesh.meshName.find("_");
            if (underscorePos != std::string::npos) {
                size_t xPos = mesh.meshName.find("x", underscorePos);
                if (xPos != std::string::npos) {
                    try {
                        radius = std::stof(mesh.meshName.substr(underscorePos + 1, xPos - underscorePos - 1));
                        height = std::stof(mesh.meshName.substr(xPos + 1));
                    } catch (...) {
                        LOG_WARNING("Failed to parse radius/height from cone mesh name: " + mesh.meshName);
                    }
                }
            }
            raylibMesh = GenMeshCone(radius, height, 16);
            LOG_INFO("✅ Generated cone mesh (radius: " + std::to_string(radius) + ", height: " + std::to_string(height) + ")");
        }
        else {
            LOG_WARNING("Unknown primitive shape: " + mesh.primitiveShape + ", defaulting to cube");
            raylibMesh = GenMeshCube(1.0f, 1.0f, 1.0f);
        }
    }
    else if (mesh.meshType == MeshComponent::MeshType::MODEL) {
        if (mesh.vertices.empty() || mesh.triangles.empty()) {
            LOG_WARNING("Custom model mesh has no geometry data, falling back to cube");
            raylibMesh = GenMeshCube(1.0f, 1.0f, 1.0f);
        } else {
            LOG_INFO("✅ Converting custom MODEL mesh: " + mesh.meshName +
                     " (" + std::to_string(mesh.vertices.size()) + " verts, " +
                     std::to_string(mesh.triangles.size()) + " tris)");

            raylibMesh = {0};
            raylibMesh.vertexCount = static_cast<int>(mesh.vertices.size());
            raylibMesh.triangleCount = static_cast<int>(mesh.triangles.size());

            raylibMesh.vertices = (float*)RL_CALLOC(mesh.vertices.size() * 3, sizeof(float));
            raylibMesh.normals = (float*)RL_CALLOC(mesh.vertices.size() * 3, sizeof(float));
            raylibMesh.texcoords = (float*)RL_CALLOC(mesh.vertices.size() * 2, sizeof(float));
            raylibMesh.colors = (unsigned char*)RL_CALLOC(mesh.vertices.size() * 4, sizeof(unsigned char));
            raylibMesh.indices = (unsigned short*)RL_CALLOC(mesh.triangles.size() * 3, sizeof(unsigned short));

            for (size_t i = 0; i < mesh.vertices.size(); ++i) {
                const auto& v = mesh.vertices[i];
                raylibMesh.vertices[i * 3 + 0] = v.position.x;
                raylibMesh.vertices[i * 3 + 1] = v.position.y;
                raylibMesh.vertices[i * 3 + 2] = v.position.z;

                raylibMesh.normals[i * 3 + 0] = v.normal.x;
                raylibMesh.normals[i * 3 + 1] = v.normal.y;
                raylibMesh.normals[i * 3 + 2] = v.normal.z;

                raylibMesh.texcoords[i * 2 + 0] = v.texCoord.x;
                raylibMesh.texcoords[i * 2 + 1] = v.texCoord.y;

                raylibMesh.colors[i * 4 + 0] = v.color.r;
                raylibMesh.colors[i * 4 + 1] = v.color.g;
                raylibMesh.colors[i * 4 + 2] = v.color.b;
                raylibMesh.colors[i * 4 + 3] = v.color.a;
            }

            for (size_t i = 0; i < mesh.triangles.size(); ++i) {
                const auto& tri = mesh.triangles[i];
                raylibMesh.indices[i * 3 + 0] = static_cast<unsigned short>(tri.v1);
                raylibMesh.indices[i * 3 + 1] = static_cast<unsigned short>(tri.v2);
                raylibMesh.indices[i * 3 + 2] = static_cast<unsigned short>(tri.v3);
            }

            UploadMesh(&raylibMesh, false);
        }
    }
    else {
        LOG_WARNING("Unknown mesh type, defaulting to cube");
        raylibMesh = GenMeshCube(1.0f, 1.0f, 1.0f);
    }
    
    if (raylibMesh.vertexCount == 0) {
        LOG_WARNING("Generated mesh has no vertices");
        return nullptr;
    }
    
    // Convert the mesh to a model
    modelData->model = LoadModelFromMesh(raylibMesh);
    modelData->isStatic = mesh.isStatic;
    modelData->lastAccessFrame = 0;
    
    LOG_INFO("✅ Created " + mesh.primitiveShape + " model (" + std::to_string(raylibMesh.vertexCount) + " vertices)");
    
    return modelData;
}

uint64_t ModelCacheFactory::CalculateMeshHash(const MeshComponent& mesh) {
    // Simple hash based on mesh properties
    size_t hash = 0;
    
    // Hash mesh type and shape
    hash ^= std::hash<int>{}(static_cast<int>(mesh.meshType)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= std::hash<std::string>{}(mesh.primitiveShape) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= std::hash<std::string>{}(mesh.meshName) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    
    // For composite meshes, include the composite ID
    if (mesh.meshType == MeshComponent::MeshType::COMPOSITE) {
        hash ^= std::hash<uint64_t>{}(mesh.compositeMeshId) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
    
    // For custom meshes, hash the vertex/triangle data
    if (mesh.meshType == MeshComponent::MeshType::MODEL) {
        hash ^= std::hash<size_t>{}(mesh.vertices.size()) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<size_t>{}(mesh.triangles.size()) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        
        // Hash first few vertices for uniqueness (avoid hashing all for performance)
        for (size_t i = 0; i < std::min(mesh.vertices.size(), size_t(4)); ++i) {
            const auto& v = mesh.vertices[i];
            hash ^= std::hash<float>{}(v.position.x) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= std::hash<float>{}(v.position.y) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= std::hash<float>{}(v.position.z) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        }
    }
    
    return hash;
}

//=============================================================================
// MATERIAL CACHE FACTORY IMPLEMENTATIONS
//=============================================================================

MaterialCacheKey MaterialCacheFactory::GenerateKey(const MaterialProperties& props) {
    MaterialCacheKey key;
    key.primaryColor = props.primaryColor;
    key.secondaryColor = props.secondaryColor;
    key.specularColor = props.specularColor;
    key.shininess = props.shininess;
    key.alpha = props.alpha;
    key.roughness = props.roughness;
    key.metallic = props.metallic;
    key.ao = props.ao;
    key.emissiveColor = props.emissiveColor;
    key.emissiveIntensity = props.emissiveIntensity;
    key.materialType = static_cast<int>(props.type);
    key.diffuseMap = props.diffuseMap;
    key.normalMap = props.normalMap;
    key.specularMap = props.specularMap;
    key.roughnessMap = props.roughnessMap;
    key.metallicMap = props.metallicMap;
    key.aoMap = props.aoMap;
    key.emissiveMap = props.emissiveMap;
    key.doubleSided = props.doubleSided;
    key.depthWrite = props.depthWrite;
    key.depthTest = props.depthTest;
    key.castShadows = props.castShadows;
    key.materialName = props.materialName;
    
    return key;
}

std::unique_ptr<CachedMaterialData> MaterialCacheFactory::CreateMaterialData(const MaterialProperties& props) {
    auto materialData = std::make_unique<CachedMaterialData>();
    
    // Copy properties to material data
    materialData->primaryColor = props.primaryColor;
    materialData->secondaryColor = props.secondaryColor;
    materialData->specularColor = props.specularColor;
    materialData->shininess = props.shininess;
    materialData->alpha = props.alpha;
    materialData->roughness = props.roughness;
    materialData->metallic = props.metallic;
    materialData->ao = props.ao;
    materialData->emissiveColor = props.emissiveColor;
    materialData->emissiveIntensity = props.emissiveIntensity;
    materialData->type = props.type;
    materialData->diffuseMap = props.diffuseMap;
    materialData->normalMap = props.normalMap;
    materialData->specularMap = props.specularMap;
    materialData->roughnessMap = props.roughnessMap;
    materialData->metallicMap = props.metallicMap;
    materialData->aoMap = props.aoMap;
    materialData->emissiveMap = props.emissiveMap;
    materialData->doubleSided = props.doubleSided;
    materialData->depthWrite = props.depthWrite;
    materialData->depthTest = props.depthTest;
    materialData->castShadows = props.castShadows;
    materialData->materialName = props.materialName;
    
    LOG_DEBUG("Created material data: " + props.materialName);
    
    return materialData;
}

//=============================================================================
// LIGHT CACHE FACTORY IMPLEMENTATIONS
//=============================================================================

#include "../Components/LightComponent.h"

// LightCacheKey implementations
bool LightCacheKey::operator==(const LightCacheKey& other) const {
    return type == other.type &&
           color.r == other.color.r && color.g == other.color.g &&
           color.b == other.color.b && color.a == other.color.a &&
           fabsf(intensity - other.intensity) < 0.001f &&
           fabsf(radius - other.radius) < 0.001f &&
           fabsf(range - other.range) < 0.001f &&
           fabsf(innerAngle - other.innerAngle) < 0.001f &&
           fabsf(outerAngle - other.outerAngle) < 0.001f &&
           castShadows == other.castShadows;
}

// Hash specialization implementation
size_t std::hash<LightCacheKey>::operator()(const LightCacheKey& key) const {
    size_t seed = 0;
    
    // Hash type
    seed ^= hash<int>{}(static_cast<int>(key.type)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    
    // Hash color components
    seed ^= hash<unsigned char>{}(key.color.r) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= hash<unsigned char>{}(key.color.g) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= hash<unsigned char>{}(key.color.b) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= hash<unsigned char>{}(key.color.a) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    
    // Hash float properties (convert to int for consistent hashing)
    seed ^= hash<uint32_t>{}(*(uint32_t*)&key.intensity) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= hash<uint32_t>{}(*(uint32_t*)&key.radius) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= hash<uint32_t>{}(*(uint32_t*)&key.range) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= hash<uint32_t>{}(*(uint32_t*)&key.innerAngle) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= hash<uint32_t>{}(*(uint32_t*)&key.outerAngle) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    
    // Hash boolean
    seed ^= hash<bool>{}(key.castShadows) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    
    return seed;
}

// LightCacheFactory implementations
LightCacheKey LightCacheFactory::GenerateKey(const LightComponent& lightComp) {
    LightCacheKey key;
    key.type = lightComp.type;
    key.color = lightComp.color;
    key.intensity = lightComp.intensity;
    key.radius = lightComp.radius;
    key.range = lightComp.range;
    key.innerAngle = lightComp.innerAngle;
    key.outerAngle = lightComp.outerAngle;
    key.castShadows = lightComp.castShadows;
    return key;
}

std::unique_ptr<CachedLightData> LightCacheFactory::CreateLightData(const LightComponent& lightComp) {
    auto lightData = std::make_unique<CachedLightData>();
    
    // Convert LightType to Raylib constants
    switch (lightComp.type) {
        case LightType::DIRECTIONAL:
            lightData->raylibLight.type = 0;
            break;
        case LightType::POINT:
            lightData->raylibLight.type = 1;
            break;
        case LightType::SPOT:
            lightData->raylibLight.type = 2;
            break;
    }
    
    lightData->raylibLight.enabled = lightComp.enabled ? 1 : 0;
    lightData->raylibLight.position = {0, 0, 0};  // Will be updated from transform
    lightData->raylibLight.target = {0, 0, 0};    // Will be updated from transform
    
    // Normalize color to 0-1 range
    lightData->raylibLight.color[0] = lightComp.color.r / 255.0f;
    lightData->raylibLight.color[1] = lightComp.color.g / 255.0f;
    lightData->raylibLight.color[2] = lightComp.color.b / 255.0f;
    lightData->raylibLight.color[3] = lightComp.color.a / 255.0f;
    
    lightData->raylibLight.attenuation = lightComp.intensity;
    lightData->isDirty = true;
    
    LOG_DEBUG("🔆 Created cached light data: type=" + std::to_string(lightData->raylibLight.type) + 
              ", attenuation=" + std::to_string(lightData->raylibLight.attenuation));
    
    return lightData;
}
