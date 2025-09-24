#include "MaterialSystem.h"
#include "AssetSystem.h"
#include "../../shaders/ShaderSystem.h"
#include "../../core/Engine.h"
#include "../../utils/Logger.h"
#include <algorithm>

MaterialSystem::MaterialSystem()
    : assetSystem_(nullptr), shaderSystem_(nullptr), 
      materialCache_(std::make_unique<MaterialCache>(
          MaterialCacheFactory::GenerateKey, MaterialCacheFactory::CreateMaterialData, "MaterialCache")),
      staticTexturesInitialized_(false) {
    LOG_INFO("MaterialSystem created with CacheSystem");
}

MaterialSystem::~MaterialSystem() {
    // Cleanup Raylib Materials - check if they're still valid
    for (auto& pair : raylibMaterialCache_) {
        // Only unload if material has valid texture IDs
        if (pair.second.maps && pair.second.maps[MATERIAL_MAP_DIFFUSE].texture.id > 0) {
            UnloadMaterial(pair.second);
        }
    }
    raylibMaterialCache_.clear();
    
    // Cleanup generated textures - check if they're still valid
    for (auto& pair : generatedTextureCache_) {
        if (pair.second.id > 0) {
            UnloadTexture(pair.second);
        }
    }
    generatedTextureCache_.clear();
    
    // Cleanup static textures - check if they're still valid
    if (staticTexturesInitialized_ && whiteDiffuse_.id > 0) {
        UnloadTexture(whiteDiffuse_);
    }
    
    LOG_INFO("MaterialSystem destroyed");
}

void MaterialSystem::Initialize() {
    LOG_INFO("MaterialSystem initialized");
    
    // Connect to AssetSystem through engine
    assetSystem_ = engine_.GetSystem<AssetSystem>();
    if (!assetSystem_) {
        LOG_ERROR("Failed to get AssetSystem reference during MaterialSystem initialization");
    } else {
        LOG_INFO("MaterialSystem connected to AssetSystem");
    }
    
    // Connect to ShaderSystem through engine
    shaderSystem_ = engine_.GetSystem<ShaderSystem>();
    if (!shaderSystem_) {
        LOG_ERROR("Failed to get ShaderSystem reference during MaterialSystem initialization");
    } else {
        LOG_INFO("MaterialSystem connected to ShaderSystem");
    }
    
    // CacheSystem handles its own stats
}

void MaterialSystem::Update(float deltaTime) {
    // Periodic cleanup of unused materials (every 10 seconds)
    static float cleanupTimer = 0.0f;
    cleanupTimer += deltaTime;

    if (cleanupTimer >= 10.0f) {
        size_t removed = CleanupUnusedMaterials();
        if (removed > 0) {
            LOG_DEBUG("Cleaned up " + std::to_string(removed) + " unused materials");
        }
        cleanupTimer = 0.0f;
    }
}

void MaterialSystem::Shutdown() {
    LOG_INFO("MaterialSystem shutting down");

    // Clear CacheSystem
    if (materialCache_) {
        materialCache_->Clear();
    }

    // Clear remaining caches  
    raylibMaterialCache_.clear();
    generatedTextureCache_.clear();

    LOG_INFO("MaterialSystem shutdown complete");
}

uint32_t MaterialSystem::GetOrCreateMaterial(const MaterialProperties& properties) {
    if (!materialCache_) {
        LOG_ERROR("MaterialCache not initialized");
        return 0;
    }

    // Use CacheSystem to get or create material
    uint32_t materialId = materialCache_->GetOrCreate(properties);
    
    LOG_DEBUG("Material cache operation - material ID " + std::to_string(materialId) + " (name: " + properties.materialName + ")");
    
    return materialId;
}

const CachedMaterialData* MaterialSystem::GetMaterial(uint32_t materialId) const {
    if (!materialCache_) {
        return nullptr;
    }
    return materialCache_->Get(materialId);
}

bool MaterialSystem::IsValidMaterialId(uint32_t materialId) const {
    if (!materialCache_) {
        return false;
    }
    return materialCache_->IsValid(materialId);
}

void MaterialSystem::AddReference(uint32_t materialId) {
    if (materialCache_ && materialCache_->IsValid(materialId)) {
        materialCache_->AddReference(materialId);
        LOG_DEBUG("Material " + std::to_string(materialId) + " ref count: " + std::to_string(materialCache_->GetRefCount(materialId)));
    }
}

bool MaterialSystem::RemoveReference(uint32_t materialId) {
    if (!materialCache_ || !materialCache_->IsValid(materialId)) {
        return false;
    }

    bool removed = materialCache_->RemoveReference(materialId);
    LOG_DEBUG("Material " + std::to_string(materialId) + " ref count: " + std::to_string(materialCache_->GetRefCount(materialId)));
    
    if (removed) {
        LOG_DEBUG("Material " + std::to_string(materialId) + " marked for cleanup (ref count = 0)");
    }

    return removed;
}

uint32_t MaterialSystem::GetReferenceCount(uint32_t materialId) const {
    if (!materialCache_) {
        return 0;
    }
    return materialCache_->GetRefCount(materialId);
}

size_t MaterialSystem::CleanupUnusedMaterials() {
    if (!materialCache_) {
        return 0;
    }
    
    size_t removed = materialCache_->CleanupUnused();
    
    if (removed > 0) {
        LOG_DEBUG("MaterialSystem cleaned up " + std::to_string(removed) + " unused materials");
    }
    
    return removed;
}


// EstimateMaterialMemory moved to CacheSystem
// EstimateMaterialMemory removed - handled by CacheSystem
/*
size_t MaterialSystem::EstimateMaterialMemory(const CachedMaterialData& material) const {
    // Rough memory estimation
    size_t memory = sizeof(CachedMaterialData);

    // Add string memory
    memory += material.materialName.capacity();
    memory += material.diffuseMap.capacity();
    memory += material.normalMap.capacity();
    memory += material.specularMap.capacity();
    memory += material.roughnessMap.capacity();
    memory += material.metallicMap.capacity();
    memory += material.aoMap.capacity();
    memory += material.emissiveMap.capacity();

    return memory;
}
*/

// =============================================================================
// Raylib Material Integration
// =============================================================================

Material MaterialSystem::GetRaylibMaterial(uint32_t materialId) {
    if (!IsValidMaterialId(materialId)) {
        LOG_WARNING("Invalid material ID: " + std::to_string(materialId) + ", returning default material");
        return LoadMaterialDefault();
    }

    const CachedMaterialData* matData = GetMaterial(materialId);
    if (!matData) {
        LOG_WARNING("MaterialData not found for ID: " + std::to_string(materialId) + ", returning default material");
        return LoadMaterialDefault();
    }

    return CreateRaylibMaterial(matData);
}

Material* MaterialSystem::GetCachedRaylibMaterial(uint32_t materialId) {
    if (!IsValidMaterialId(materialId)) {
        return nullptr;
    }

    // Check cache first
    auto it = raylibMaterialCache_.find(materialId);
    if (it != raylibMaterialCache_.end()) {
        return &it->second;
    }

    // Create and cache new material
    const CachedMaterialData* matData = GetMaterial(materialId);
    if (!matData) {
        return nullptr;
    }

    Material raylibMaterial = CreateRaylibMaterial(matData);
    raylibMaterialCache_[materialId] = raylibMaterial;
    // cacheStats_ now handled by CacheSystem
    
    return &raylibMaterialCache_[materialId];
}

void MaterialSystem::RefreshRaylibMaterialCache(uint32_t materialId) {
    if (materialId == 0) {
        // Refresh all cached materials
        for (auto& pair : raylibMaterialCache_) {
            UnloadMaterial(pair.second);
        }
        raylibMaterialCache_.clear();
        LOG_DEBUG("Refreshed all Raylib material cache");
    } else {
        // Refresh specific material
        auto it = raylibMaterialCache_.find(materialId);
        if (it != raylibMaterialCache_.end()) {
            UnloadMaterial(it->second);
            raylibMaterialCache_.erase(it);
            LOG_DEBUG("Refreshed Raylib material cache for ID: " + std::to_string(materialId));
        }
    }
}

void MaterialSystem::ApplyMaterialToModel(uint32_t materialId, Model& model, int meshIndex) {
    Material* raylibMaterial = GetCachedRaylibMaterial(materialId);
    if (!raylibMaterial) {
        LOG_WARNING("Failed to get Raylib material for ID: " + std::to_string(materialId));
        return;
    }

    // Get appropriate shader for this material
    uint32_t shaderId = 0;
    if (shaderSystem_) {
        // Apply the lighting shader to materials so entities get lighting
        shaderId = shaderSystem_->GetLightingShaderId();
    }

    if (meshIndex == -1) {
        // Apply to all meshes
        for (int i = 0; i < model.materialCount; i++) {
            Material& targetMaterial = model.materials[i];
            
            // Copy texture maps and properties WITHOUT overwriting the entire material
            targetMaterial.maps[MATERIAL_MAP_DIFFUSE] = raylibMaterial->maps[MATERIAL_MAP_DIFFUSE];
            targetMaterial.maps[MATERIAL_MAP_NORMAL] = raylibMaterial->maps[MATERIAL_MAP_NORMAL];
            targetMaterial.maps[MATERIAL_MAP_SPECULAR] = raylibMaterial->maps[MATERIAL_MAP_SPECULAR];
            targetMaterial.maps[MATERIAL_MAP_ROUGHNESS] = raylibMaterial->maps[MATERIAL_MAP_ROUGHNESS];
            targetMaterial.maps[MATERIAL_MAP_METALNESS] = raylibMaterial->maps[MATERIAL_MAP_METALNESS];
            targetMaterial.maps[MATERIAL_MAP_OCCLUSION] = raylibMaterial->maps[MATERIAL_MAP_OCCLUSION];
            targetMaterial.maps[MATERIAL_MAP_EMISSION] = raylibMaterial->maps[MATERIAL_MAP_EMISSION];
            targetMaterial.maps[MATERIAL_MAP_HEIGHT] = raylibMaterial->maps[MATERIAL_MAP_HEIGHT];
            targetMaterial.maps[MATERIAL_MAP_CUBEMAP] = raylibMaterial->maps[MATERIAL_MAP_CUBEMAP];
            targetMaterial.maps[MATERIAL_MAP_IRRADIANCE] = raylibMaterial->maps[MATERIAL_MAP_IRRADIANCE];
            targetMaterial.maps[MATERIAL_MAP_PREFILTER] = raylibMaterial->maps[MATERIAL_MAP_PREFILTER];
            targetMaterial.maps[MATERIAL_MAP_BRDF] = raylibMaterial->maps[MATERIAL_MAP_BRDF];
            
            // Copy material parameters
            for (int paramIdx = 0; paramIdx < 4; paramIdx++) {
                targetMaterial.params[paramIdx] = raylibMaterial->params[paramIdx];
            }
            
            // Apply shader AFTER texture assignment (this is the key fix!)
            if (shaderId > 0 && shaderSystem_) {
                shaderSystem_->ApplyShaderToModel(shaderId, model, i);
            }
        }
        LOG_INFO("🎨 APPLIED MATERIAL to all meshes:");
        LOG_INFO("  Material ID: " + std::to_string(materialId));
        LOG_INFO("  Shader ID: " + std::to_string(shaderId));
        LOG_INFO("  Mesh count: " + std::to_string(model.materialCount));
        
        // Verify texture assignment worked
        for (int i = 0; i < model.materialCount; i++) {
            auto& mat = model.materials[i];
            LOG_INFO("  Mesh " + std::to_string(i) + " texture ID: " + std::to_string(mat.maps[MATERIAL_MAP_DIFFUSE].texture.id));
            LOG_INFO("  Mesh " + std::to_string(i) + " shader ID: " + std::to_string(mat.shader.id));
        }
    } else if (meshIndex >= 0 && meshIndex < model.materialCount) {
        // Apply to specific mesh
        Material& targetMaterial = model.materials[meshIndex];
        
        // Copy texture maps and properties WITHOUT overwriting the entire material
        targetMaterial.maps[MATERIAL_MAP_DIFFUSE] = raylibMaterial->maps[MATERIAL_MAP_DIFFUSE];
        targetMaterial.maps[MATERIAL_MAP_NORMAL] = raylibMaterial->maps[MATERIAL_MAP_NORMAL];
        targetMaterial.maps[MATERIAL_MAP_SPECULAR] = raylibMaterial->maps[MATERIAL_MAP_SPECULAR];
        targetMaterial.maps[MATERIAL_MAP_ROUGHNESS] = raylibMaterial->maps[MATERIAL_MAP_ROUGHNESS];
        targetMaterial.maps[MATERIAL_MAP_METALNESS] = raylibMaterial->maps[MATERIAL_MAP_METALNESS];
        targetMaterial.maps[MATERIAL_MAP_OCCLUSION] = raylibMaterial->maps[MATERIAL_MAP_OCCLUSION];
        targetMaterial.maps[MATERIAL_MAP_EMISSION] = raylibMaterial->maps[MATERIAL_MAP_EMISSION];
        targetMaterial.maps[MATERIAL_MAP_HEIGHT] = raylibMaterial->maps[MATERIAL_MAP_HEIGHT];
        targetMaterial.maps[MATERIAL_MAP_CUBEMAP] = raylibMaterial->maps[MATERIAL_MAP_CUBEMAP];
        targetMaterial.maps[MATERIAL_MAP_IRRADIANCE] = raylibMaterial->maps[MATERIAL_MAP_IRRADIANCE];
        targetMaterial.maps[MATERIAL_MAP_PREFILTER] = raylibMaterial->maps[MATERIAL_MAP_PREFILTER];
        targetMaterial.maps[MATERIAL_MAP_BRDF] = raylibMaterial->maps[MATERIAL_MAP_BRDF];
        
        // Copy material parameters
        for (int paramIdx = 0; paramIdx < 4; paramIdx++) {
            targetMaterial.params[paramIdx] = raylibMaterial->params[paramIdx];
        }
        
        // Apply shader AFTER texture assignment (this is the key fix!)
        if (shaderId > 0 && shaderSystem_) {
            shaderSystem_->ApplyShaderToModel(shaderId, model, meshIndex);
        }
        
        LOG_INFO("🎨 APPLIED MATERIAL to specific mesh:");
        LOG_INFO("  Material ID: " + std::to_string(materialId));
        LOG_INFO("  Shader ID: " + std::to_string(shaderId));
        LOG_INFO("  Mesh index: " + std::to_string(meshIndex));
        
        // Verify texture assignment worked
        auto& mat = model.materials[meshIndex];
        LOG_INFO("  Final texture ID: " + std::to_string(mat.maps[MATERIAL_MAP_DIFFUSE].texture.id));
        LOG_INFO("  Final shader ID: " + std::to_string(mat.shader.id));
    } else {
        LOG_WARNING("Invalid mesh index " + std::to_string(meshIndex) + " for model with " + 
                   std::to_string(model.materialCount) + " materials");
    }
}

Material MaterialSystem::CreateRaylibMaterial(const CachedMaterialData* matData) const {
    if (!matData) {
        LOG_WARNING("CreateRaylibMaterial called with null CachedMaterialData");
        return LoadMaterialDefault();
    }

    // Initialize static textures if needed
    if (!staticTexturesInitialized_) {
        InitializeStaticTextures();
    }

    Material rayMaterial = LoadMaterialDefault();

    // Determine material application mode
    bool hasTexture = !matData->diffuseMap.empty();
    bool hasGradient = IsGradientMaterial(matData);

    if (hasTexture) {
        ApplyDiffuseTexture(rayMaterial, matData->diffuseMap);
        
        // Apply additional PBR textures if this is a PBR material
        if (matData->type == CachedMaterialData::MaterialType::PBR) {
            ApplyPBRTextures(rayMaterial, matData);
        }
    } else if (hasGradient) {
        ApplyGradientTexture(rayMaterial, matData);
    } else {
        // Solid color material
        ApplySolidColor(rayMaterial, matData->primaryColor);
    }

    // Set material properties - only for non-textured materials
    if (!hasTexture) {
        rayMaterial.maps[MATERIAL_MAP_ALBEDO].color = matData->primaryColor;
    }
    
    LOG_DEBUG("Created Raylib material: " + matData->materialName + 
             " (textured=" + (hasTexture ? "yes" : "no") + 
             ", type=" + std::to_string((int)matData->type) + ")");

    return rayMaterial;
}

bool MaterialSystem::IsGradientMaterial(const CachedMaterialData* matData) const {
    if (!matData) return false;
    return matData->secondaryColor.a > 0 && (
        matData->secondaryColor.r != matData->primaryColor.r ||
        matData->secondaryColor.g != matData->primaryColor.g ||
        matData->secondaryColor.b != matData->primaryColor.b);
}

void MaterialSystem::ApplySolidColor(Material& material, Color color) const {
    // Use cached white texture and apply color via material color
    material.maps[MATERIAL_MAP_DIFFUSE].texture = whiteDiffuse_;
    material.maps[MATERIAL_MAP_DIFFUSE].color = color;
}

void MaterialSystem::ApplyGradientTexture(Material& material, const CachedMaterialData* matData) const {
    // Generate gradient texture key
    std::string textureKey = CreateGradientTextureKey(matData->primaryColor, matData->secondaryColor, 0);
    
    // Check if gradient texture is already cached
    auto it = generatedTextureCache_.find(textureKey);
    Texture2D gradientTexture;
    
    if (it != generatedTextureCache_.end()) {
        gradientTexture = it->second;
    } else {
        // Generate new gradient texture
        gradientTexture = GenerateGradientTexture(matData->primaryColor, matData->secondaryColor, 0);
        generatedTextureCache_[textureKey] = gradientTexture;
    }
    
    material.maps[MATERIAL_MAP_DIFFUSE].texture = gradientTexture;
}

void MaterialSystem::ApplyDiffuseTexture(Material& material, const std::string& texturePath) const {
    LOG_INFO("🎨 ATTEMPTING to load texture: " + texturePath);
    
    if (!assetSystem_) {
        LOG_ERROR("❌ AssetSystem not available for texture: " + texturePath);
        ApplySolidColor(material, WHITE);
        return;
    }

    Texture2D* texture = assetSystem_->GetOrLoadTexture(texturePath);
    if (texture && texture->id != 0) {
        LOG_INFO("✅ TEXTURE LOADED SUCCESSFULLY:");
        LOG_INFO("  Path: " + texturePath);
        LOG_INFO("  ID: " + std::to_string(texture->id));
        LOG_INFO("  Size: " + std::to_string(texture->width) + "x" + std::to_string(texture->height));
        LOG_INFO("  Format: " + std::to_string(texture->format));
        LOG_INFO("  Mipmaps: " + std::to_string(texture->mipmaps));
        
        SetTextureFilter(*texture, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(*texture, TEXTURE_WRAP_CLAMP);
        
        // Proper Raylib material texture assignment
        material.maps[MATERIAL_MAP_DIFFUSE].texture = *texture;
        material.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
        
        LOG_INFO("✅ TEXTURE ASSIGNED to material DIFFUSE slot");
        LOG_INFO("  Material texture ID: " + std::to_string(material.maps[MATERIAL_MAP_DIFFUSE].texture.id));
    } else {
        LOG_ERROR("❌ TEXTURE LOADING FAILED:");
        LOG_ERROR("  Path: " + texturePath);
        LOG_ERROR("  Texture pointer: " + std::string(texture ? "valid" : "null"));
        if (texture) {
            LOG_ERROR("  Texture ID: " + std::to_string(texture->id) + " (should be > 0)");
            LOG_ERROR("  Texture dimensions: " + std::to_string(texture->width) + "x" + std::to_string(texture->height));
        }
        LOG_WARNING("  Falling back to solid color material");
        ApplySolidColor(material, WHITE);
    }
}

void MaterialSystem::ApplyPBRTextures(Material& material, const CachedMaterialData* matData) const {
    if (!assetSystem_) {
        LOG_WARNING("AssetSystem not available for PBR texture loading");
        return;
    }

    // Apply additional PBR texture maps using direct assignment
    if (!matData->normalMap.empty()) {
        Texture2D* normalTexture = assetSystem_->GetOrLoadTexture(matData->normalMap);
        if (normalTexture && normalTexture->id != 0) {
            material.maps[MATERIAL_MAP_NORMAL].texture = *normalTexture;
            LOG_DEBUG("Applied normal map: " + matData->normalMap);
        }
    }

    if (!matData->specularMap.empty()) {
        Texture2D* specularTexture = assetSystem_->GetOrLoadTexture(matData->specularMap);
        if (specularTexture && specularTexture->id != 0) {
            material.maps[MATERIAL_MAP_SPECULAR].texture = *specularTexture;
            LOG_DEBUG("Applied specular map: " + matData->specularMap);
        }
    }

    if (!matData->roughnessMap.empty()) {
        Texture2D* roughnessTexture = assetSystem_->GetOrLoadTexture(matData->roughnessMap);
        if (roughnessTexture && roughnessTexture->id != 0) {
            material.maps[MATERIAL_MAP_ROUGHNESS].texture = *roughnessTexture;
            LOG_DEBUG("Applied roughness map: " + matData->roughnessMap);
        }
    }

    if (!matData->metallicMap.empty()) {
        Texture2D* metallicTexture = assetSystem_->GetOrLoadTexture(matData->metallicMap);
        if (metallicTexture && metallicTexture->id != 0) {
            material.maps[MATERIAL_MAP_METALNESS].texture = *metallicTexture;
            LOG_DEBUG("Applied metallic map: " + matData->metallicMap);
        }
    }

    if (!matData->aoMap.empty()) {
        Texture2D* aoTexture = assetSystem_->GetOrLoadTexture(matData->aoMap);
        if (aoTexture && aoTexture->id != 0) {
            material.maps[MATERIAL_MAP_OCCLUSION].texture = *aoTexture;
            LOG_DEBUG("Applied AO map: " + matData->aoMap);
        }
    }

    if (!matData->emissiveMap.empty()) {
        Texture2D* emissiveTexture = assetSystem_->GetOrLoadTexture(matData->emissiveMap);
        if (emissiveTexture && emissiveTexture->id != 0) {
            material.maps[MATERIAL_MAP_EMISSION].texture = *emissiveTexture;
            LOG_DEBUG("Applied emissive map: " + matData->emissiveMap);
        }
    }
}

Texture2D MaterialSystem::GenerateGradientTexture(Color primary, Color secondary, uint16_t gradientMode) const {
    // Generate a simple 64x64 gradient texture
    const int size = 64;
    Image gradientImage = GenImageGradientLinear(size, size, 0, primary, secondary); // TODO: Use gradientMode
    Texture2D gradientTexture = LoadTextureFromImage(gradientImage);
    UnloadImage(gradientImage);
    
    SetTextureFilter(gradientTexture, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(gradientTexture, TEXTURE_WRAP_REPEAT);
    
    return gradientTexture;
}

void MaterialSystem::InitializeStaticTextures() const {
    if (staticTexturesInitialized_) {
        return;
    }

    // Create 1x1 white texture for solid color materials
    Image whiteImage = GenImageColor(1, 1, WHITE);
    whiteDiffuse_ = LoadTextureFromImage(whiteImage);
    UnloadImage(whiteImage);
    
    SetTextureFilter(whiteDiffuse_, TEXTURE_FILTER_POINT);
    SetTextureWrap(whiteDiffuse_, TEXTURE_WRAP_REPEAT);
    
    staticTexturesInitialized_ = true;
    LOG_DEBUG("Initialized static textures for MaterialSystem");
}

std::string MaterialSystem::CreateGradientTextureKey(Color primary, Color secondary, uint16_t gradientMode) const {
    return "gradient_" + 
           std::to_string(primary.r) + "_" + std::to_string(primary.g) + "_" + 
           std::to_string(primary.b) + "_" + std::to_string(primary.a) + "_" +
           std::to_string(secondary.r) + "_" + std::to_string(secondary.g) + "_" + 
           std::to_string(secondary.b) + "_" + std::to_string(secondary.a) + "_" +
           std::to_string(gradientMode);
}
