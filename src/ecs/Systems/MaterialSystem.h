#pragma once

#include "../System.h"
#include "../Systems/AssetSystem.h"
#include "CacheSystem.h"
#include "raylib.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <string>

// Forward declarations
class ShaderSystem;

// MaterialData moved to CacheSystem.h as CachedMaterialData

/**
 * @brief Material Properties - Input for material creation
 *
 * This struct is used as input to GetOrCreateMaterial() to specify
 * the desired material properties. It represents the "extrinsic state"
 * that can be passed to the flyweight factory.
 */
// MaterialProperties and MaterialData moved to CacheSystem.h to avoid redefinition
// MaterialData typedef to new type for backward compatibility
using MaterialData = CachedMaterialData;

/**
 * @brief Material Key - Used for deduplication in lookup map
 *
 * This struct provides a hashable key for material deduplication.
 * Two materials with the same key are considered identical and will
 * share the same MaterialData instance.
 */
struct MaterialKey {
    // Core identifying properties
    Color primaryColor;
    Color secondaryColor;
    Color specularColor;
    float shininess;
    float alpha;
    float roughness;
    float metallic;
    float ao;
    Color emissiveColor;
    float emissiveIntensity;
    MaterialData::MaterialType type;

    // Texture maps (empty string if no texture)
    std::string diffuseMap;
    std::string normalMap;
    std::string specularMap;
    std::string roughnessMap;
    std::string metallicMap;
    std::string aoMap;
    std::string emissiveMap;

    // Rendering flags
    bool doubleSided;
    bool depthWrite;
    bool depthTest;
    bool castShadows;

    // Equality operator for unordered_map
    bool operator==(const MaterialKey& other) const {
        return primaryColor.r == other.primaryColor.r &&
               primaryColor.g == other.primaryColor.g &&
               primaryColor.b == other.primaryColor.b &&
               primaryColor.a == other.primaryColor.a &&
               secondaryColor.r == other.secondaryColor.r &&
               secondaryColor.g == other.secondaryColor.g &&
               secondaryColor.b == other.secondaryColor.b &&
               secondaryColor.a == other.secondaryColor.a &&
               specularColor.r == other.specularColor.r &&
               specularColor.g == other.specularColor.g &&
               specularColor.b == other.specularColor.b &&
               specularColor.a == other.specularColor.a &&
               shininess == other.shininess &&
               alpha == other.alpha &&
               roughness == other.roughness &&
               metallic == other.metallic &&
               ao == other.ao &&
               emissiveColor.r == other.emissiveColor.r &&
               emissiveColor.g == other.emissiveColor.g &&
               emissiveColor.b == other.emissiveColor.b &&
               emissiveColor.a == other.emissiveColor.a &&
               emissiveIntensity == other.emissiveIntensity &&
               type == other.type &&
               diffuseMap == other.diffuseMap &&
               normalMap == other.normalMap &&
               specularMap == other.specularMap &&
               roughnessMap == other.roughnessMap &&
               metallicMap == other.metallicMap &&
               aoMap == other.aoMap &&
               emissiveMap == other.emissiveMap &&
               doubleSided == other.doubleSided &&
               depthWrite == other.depthWrite &&
               depthTest == other.depthTest &&
               castShadows == other.castShadows;
    }
};

// Hash function for MaterialKey (required for unordered_map)
// Using boost::hash_combine style algorithm for better collision resistance
namespace std {
    template <>
    struct hash<MaterialKey> {
        size_t operator()(const MaterialKey& key) const {
            // Use boost::hash_combine algorithm: hash = hash ^ (value + 0x9e3779b9 + (hash << 6) + (hash >> 2))
            size_t seed = 0;

            // Colors as packed values
            uint32_t primaryPacked = (key.primaryColor.r) | (key.primaryColor.g << 8) |
                                   (key.primaryColor.b << 16) | (key.primaryColor.a << 24);
            uint32_t secondaryPacked = (key.secondaryColor.r) | (key.secondaryColor.g << 8) |
                                     (key.secondaryColor.b << 16) | (key.secondaryColor.a << 24);
            uint32_t specularPacked = (key.specularColor.r) | (key.specularColor.g << 8) |
                                    (key.specularColor.b << 16) | (key.specularColor.a << 24);
            uint32_t emissivePacked = (key.emissiveColor.r) | (key.emissiveColor.g << 8) |
                                    (key.emissiveColor.b << 16) | (key.emissiveColor.a << 24);

            hash_combine(seed, primaryPacked);
            hash_combine(seed, secondaryPacked);
            hash_combine(seed, specularPacked);
            hash_combine(seed, emissivePacked);

            // Float values (bit-perfect hashing)
            hash_combine(seed, *reinterpret_cast<const uint32_t*>(&key.shininess));
            hash_combine(seed, *reinterpret_cast<const uint32_t*>(&key.alpha));
            hash_combine(seed, *reinterpret_cast<const uint32_t*>(&key.roughness));
            hash_combine(seed, *reinterpret_cast<const uint32_t*>(&key.metallic));
            hash_combine(seed, *reinterpret_cast<const uint32_t*>(&key.ao));
            hash_combine(seed, *reinterpret_cast<const uint32_t*>(&key.emissiveIntensity));

            // Enum and flags
            hash_combine(seed, static_cast<size_t>(key.type));
            uint8_t flags = (key.doubleSided ? 1 : 0) | ((key.depthWrite ? 1 : 0) << 1) |
                           ((key.depthTest ? 1 : 0) << 2) | ((key.castShadows ? 1 : 0) << 3);
            hash_combine(seed, flags);

            // Strings
            hash_combine(seed, key.diffuseMap);
            hash_combine(seed, key.normalMap);
            hash_combine(seed, key.specularMap);
            hash_combine(seed, key.roughnessMap);
            hash_combine(seed, key.metallicMap);
            hash_combine(seed, key.aoMap);
            hash_combine(seed, key.emissiveMap);

            return seed;
        }

    private:
        // boost::hash_combine implementation
        template <class T>
        void hash_combine(size_t& seed, const T& v) const {
            seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
    };
}

/**
 * @brief Material System - Flyweight Pattern Implementation
 *
 * This system implements the flyweight pattern for materials, providing
 * centralized material data management with automatic deduplication.
 * MaterialComponents become lightweight handles that reference shared data.
 *
 * Features:
 * - Automatic material deduplication based on properties
 * - Reference counting for automatic cleanup
 * - Efficient material lookup and caching
 * - Clean separation between material handles and data
 */
class MaterialSystem : public System {
public:
    MaterialSystem();
    ~MaterialSystem() override;

    // System interface
    void Initialize() override;
    void Update(float deltaTime) override;
    void Shutdown() override;
    const char* GetName() const { return "MaterialSystem"; }

    // Material Management
    // ===================

    /**
     * @brief Get or create a material (main interface - now uses CacheSystem)
     *
     * This is the primary method for getting materials. Uses the new CacheSystem
     * for consistent caching patterns across all systems.
     *
     * @param properties Material properties to match/create
     * @return Material ID (cache ID from CacheSystem)
     */
    uint32_t GetOrCreateMaterial(const MaterialProperties& properties);

    /**
     * @brief Get material data by ID
     *
     * @param materialId Material ID returned by GetOrCreateMaterial
     * @return Pointer to material data, or nullptr if invalid ID
     */
    const CachedMaterialData* GetMaterial(uint32_t materialId) const;

    /**
     * @brief Check if a material ID is valid
     *
     * @param materialId Material ID to check
     * @return true if valid, false otherwise
     */
    bool IsValidMaterialId(uint32_t materialId) const;

    /**
     * @brief Get the total number of unique materials
     *
     * @return Number of materials in the cache
     */
    size_t GetMaterialCount() const { return materialCache_->Size(); }

    // Reference Counting
    // ==================

    /**
     * @brief Increment reference count for a material
     *
     * @param materialId Material ID to increment
     */
    void AddReference(uint32_t materialId);

    /**
     * @brief Decrement reference count for a material
     *
     * @param materialId Material ID to decrement
     * @return true if material was removed (ref count reached 0), false otherwise
     */
    bool RemoveReference(uint32_t materialId);

    /**
     * @brief Get reference count for a material
     *
     * @param materialId Material ID to check
     * @return Reference count, or 0 if invalid ID
     */
    uint32_t GetReferenceCount(uint32_t materialId) const;

    // Cleanup
    // =======

    /**
     * @brief Remove materials with zero references
     *
     * @return Number of materials removed
     */
    size_t CleanupUnusedMaterials();

    // Raylib Material Integration
    // ==========================

    /**
     * @brief Convert MaterialData to Raylib Material
     * 
     * This is the core method that bridges our MaterialSystem with Raylib's rendering.
     * It handles texture loading, solid colors, gradients, and PBR materials.
     *
     * @param materialId Material ID to convert
     * @return Raylib Material ready for rendering, or default material if invalid ID
     */
    Material GetRaylibMaterial(uint32_t materialId);

    /**
     * @brief Get cached Raylib Material (performance optimized)
     *
     * Returns a cached Raylib Material if available, otherwise creates and caches it.
     * This is the recommended method for rendering as it avoids redundant conversions.
     *
     * @param materialId Material ID to get
     * @return Pointer to cached Raylib Material, or nullptr if invalid ID
     */
    Material* GetCachedRaylibMaterial(uint32_t materialId);

    /**
     * @brief Force refresh of Raylib Material cache
     *
     * Call this when MaterialData properties have changed and need to be
     * reflected in the Raylib Material.
     *
     * @param materialId Material ID to refresh, or 0 to refresh all
     */
    void RefreshRaylibMaterialCache(uint32_t materialId = 0);

    /**
     * @brief Apply material to Raylib Model
     *
     * Convenience method that applies a material to all meshes in a Raylib Model.
     *
     * @param materialId Material ID to apply
     * @param model Raylib Model to modify
     * @param meshIndex Specific mesh index, or -1 for all meshes
     */
    void ApplyMaterialToModel(uint32_t materialId, Model& model, int meshIndex = -1);

    // Statistics
    // ==========

    /**
     * @brief Get cache statistics
     */
    struct CacheStats {
        size_t totalMaterials = 0;
        size_t totalRequests = 0;
        size_t cacheHits = 0;
        size_t cacheMisses = 0;
        size_t memoryUsed = 0;
        size_t raylibMaterialsCreated = 0;
        size_t raylibMaterialsCached = 0;

        float GetHitRate() const {
            return totalRequests > 0 ? (float)cacheHits / totalRequests : 0.0f;
        }
        
        float GetRaylibCacheEfficiency() const {
            return raylibMaterialsCreated > 0 ? (float)raylibMaterialsCached / raylibMaterialsCreated : 0.0f;
        }
    };

    CacheStats GetCacheStats() const { 
        if (materialCache_) {
            // Convert CacheSystem::CacheStats to MaterialSystem::CacheStats
            auto stats = materialCache_->GetStats();
            CacheStats materialStats;
            materialStats.totalRequests = stats.totalRequests;
            materialStats.cacheHits = stats.cacheHits;
            materialStats.cacheMisses = stats.cacheMisses;
            materialStats.totalMaterials = stats.totalRequests;
            materialStats.memoryUsed = stats.memoryUsed;
            return materialStats;
        }
        return CacheStats{};
    }

    // AssetSystem initialization (called during Engine setup)
    void SetAssetSystem(AssetSystem* assetSystem) { assetSystem_ = assetSystem; }
    
    // Direct cache access (consistent with ModelCache pattern)
    MaterialCache* GetMaterialCache() { return materialCache_.get(); }

private:
    // System pointers
    AssetSystem* assetSystem_;
    ShaderSystem* shaderSystem_;

    // Material caching using CacheSystem
    std::unique_ptr<MaterialCache> materialCache_;

    // Raylib Material cache (materialId -> Raylib Material)
    mutable std::unordered_map<uint32_t, Material> raylibMaterialCache_;
    
    // Generated texture cache for gradients and solid colors
    mutable std::unordered_map<std::string, Texture2D> generatedTextureCache_;
    
    // Static default textures
    mutable Texture2D whiteDiffuse_;
    mutable bool staticTexturesInitialized_;

    // Helper methods (moved to CacheSystem)
    
    // Raylib Material conversion helpers
    Material CreateRaylibMaterial(const CachedMaterialData* matData) const;
    bool IsGradientMaterial(const CachedMaterialData* matData) const;
    void ApplySolidColor(Material& material, Color color) const;
    void ApplyGradientTexture(Material& material, const CachedMaterialData* matData) const;
    void ApplyDiffuseTexture(Material& material, const std::string& texturePath) const;
    void ApplyPBRTextures(Material& material, const CachedMaterialData* matData) const;
    Texture2D GenerateGradientTexture(Color primary, Color secondary, uint16_t gradientMode) const;
    void InitializeStaticTextures() const;
    std::string CreateGradientTextureKey(Color primary, Color secondary, uint16_t gradientMode) const;
};
