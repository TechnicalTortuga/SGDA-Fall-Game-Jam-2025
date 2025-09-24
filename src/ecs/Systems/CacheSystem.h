#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>
#include "../../utils/Logger.h"
#include "raylib.h"

// Forward declarations
struct MeshComponent;

/**
 * @brief Generic Cache System - Flyweight Pattern Implementation
 * 
 * This system provides generic caching functionality with deduplication,
 * reference counting, and automatic cleanup for any type of cached object.
 * 
 * Template Parameters:
 * - TKey: The key type for lookups (must be hashable)
 * - TData: The data type being cached
 * - TProperties: The properties type used to create data
 */
template<typename TKey, typename TData, typename TProperties>
class CacheSystem {
public:
    using DataPtr = std::unique_ptr<TData>;
    using KeyGenerator = std::function<TKey(const TProperties&)>;
    using DataFactory = std::function<DataPtr(const TProperties&)>;

    struct CacheStats {
        size_t totalItems = 0;
        size_t totalRequests = 0;
        size_t cacheHits = 0;
        size_t cacheMisses = 0;
        size_t memoryUsed = 0;
        size_t cleanupRuns = 0;
        size_t itemsRemoved = 0;

        float GetHitRate() const {
            return totalRequests > 0 ? (float)cacheHits / totalRequests : 0.0f;
        }
    };

    /**
     * @brief Constructor
     * @param keyGen Function to generate cache keys from properties
     * @param dataFactory Function to create new data from properties
     * @param name Cache name for logging
     */
    CacheSystem(KeyGenerator keyGen, DataFactory dataFactory, const std::string& name = "Cache")
        : keyGenerator_(keyGen)
        , dataFactory_(dataFactory)
        , cacheName_(name)
        , nextId_(1) {
        LOG_DEBUG("Created " + cacheName_ + " cache system");
    }

    ~CacheSystem() {
        Clear();
        LOG_DEBUG("Destroyed " + cacheName_ + " cache system");
    }

    /**
     * @brief Get or create cached item (main interface)
     * @param properties Properties to match/create
     * @return Item ID for accessing the cached data
     */
    uint32_t GetOrCreate(const TProperties& properties) {
        stats_.totalRequests++;

        // Generate lookup key
        TKey key = keyGenerator_(properties);

        // Check if item already exists
        auto it = lookupMap_.find(key);
        if (it != lookupMap_.end()) {
            // Cache hit - increment reference count
            uint32_t id = it->second;
            AddReference(id);
            stats_.cacheHits++;
            LOG_DEBUG(cacheName_ + " cache HIT for ID " + std::to_string(id));
            return id;
        }

        // Cache miss - create new item
        stats_.cacheMisses++;
        uint32_t newId = CreateNewItem(properties, key);
        LOG_DEBUG(cacheName_ + " cache MISS - created new item ID " + std::to_string(newId));
        return newId;
    }

    /**
     * @brief Get cached data by ID
     * @param id Item ID
     * @return Pointer to data, or nullptr if invalid
     */
    const TData* Get(uint32_t id) const {
        if (id == 0 || id >= data_.size() || !data_[id]) {
            return nullptr;
        }
        return data_[id].get();
    }

    /**
     * @brief Get mutable cached data by ID
     * @param id Item ID
     * @return Mutable pointer to data, or nullptr if invalid
     */
    TData* GetMutable(uint32_t id) {
        if (id == 0 || id >= data_.size() || !data_[id]) {
            return nullptr;
        }
        return data_[id].get();
    }

    /**
     * @brief Check if ID is valid
     * @param id Item ID to check
     * @return true if valid, false otherwise
     */
    bool IsValid(uint32_t id) const {
        return id > 0 && id < data_.size() && data_[id] != nullptr;
    }

    /**
     * @brief Add reference to cached item
     * @param id Item ID
     */
    void AddReference(uint32_t id) {
        if (IsValid(id)) {
            refCounts_[id]++;
        }
    }

    /**
     * @brief Remove reference from cached item
     * @param id Item ID
     * @return true if item was removed (ref count reached 0)
     */
    bool RemoveReference(uint32_t id) {
        if (!IsValid(id)) return false;

        if (refCounts_[id] > 0) {
            refCounts_[id]--;
        }

        // If reference count reaches 0, mark for cleanup
        if (refCounts_[id] == 0) {
            LOG_DEBUG(cacheName_ + " item ID " + std::to_string(id) + " marked for cleanup (ref count = 0)");
            return true;
        }

        return false;
    }

    /**
     * @brief Get reference count for item
     * @param id Item ID
     * @return Reference count
     */
    uint32_t GetRefCount(uint32_t id) const {
        return IsValid(id) ? refCounts_[id] : 0;
    }

    /**
     * @brief Clean up unused items (ref count = 0)
     * @return Number of items removed
     */
    size_t CleanupUnused() {
        size_t removed = 0;
        stats_.cleanupRuns++;

        for (uint32_t id = 1; id < data_.size(); ++id) {
            if (data_[id] && refCounts_[id] == 0) {
                // Remove from lookup map
                for (auto it = lookupMap_.begin(); it != lookupMap_.end(); ++it) {
                    if (it->second == id) {
                        lookupMap_.erase(it);
                        break;
                    }
                }

                // Clear data
                data_[id].reset();
                removed++;
            }
        }

        if (removed > 0) {
            stats_.itemsRemoved += removed;
            stats_.totalItems -= removed;
            LOG_DEBUG(cacheName_ + " cleanup: removed " + std::to_string(removed) + " unused items");
        }

        return removed;
    }

    /**
     * @brief Clear all cached items
     */
    void Clear() {
        data_.clear();
        refCounts_.clear();
        lookupMap_.clear();
        stats_.totalItems = 0;
        nextId_ = 1;
        LOG_INFO("Cleared " + cacheName_ + " cache");
    }

    /**
     * @brief Get cache statistics
     */
    const CacheStats& GetStats() const { return stats_; }

    /**
     * @brief Reset cache statistics
     */
    void ResetStats() {
        stats_ = CacheStats{};
        stats_.totalItems = data_.size();
    }

    /**
     * @brief Get total number of cached items
     */
    size_t Size() const { return stats_.totalItems; }

private:
    uint32_t CreateNewItem(const TProperties& properties, const TKey& key) {
        // Create new data using factory
        auto newData = dataFactory_(properties);
        if (!newData) {
            LOG_ERROR(cacheName_ + " factory failed to create data");
            return 0;
        }

        // Assign new ID
        uint32_t newId = nextId_++;

        // Ensure vectors are large enough
        if (newId >= data_.size()) {
            data_.resize(newId + 1);
            refCounts_.resize(newId + 1);
        }

        // Store data
        data_[newId] = std::move(newData);
        refCounts_[newId] = 1; // Start with 1 reference

        // Add to lookup map
        lookupMap_[key] = newId;

        stats_.totalItems++;
        return newId;
    }

    // Function objects for key generation and data creation
    KeyGenerator keyGenerator_;
    DataFactory dataFactory_;

    // Cache name for logging
    std::string cacheName_;

    // Data storage
    std::vector<DataPtr> data_;
    std::vector<uint32_t> refCounts_;
    std::unordered_map<TKey, uint32_t> lookupMap_;

    // ID generation
    uint32_t nextId_;

    // Statistics
    CacheStats stats_;
};

//=============================================================================
// MODEL CACHING SPECIALIZATION
//=============================================================================

/**
 * @brief Model cache key for Raylib Model caching
 */
struct ModelCacheKey {
    uint64_t meshHash;
    
    bool operator==(const ModelCacheKey& other) const {
        return meshHash == other.meshHash;
    }
};

// Hash function for ModelCacheKey
namespace std {
    template <>
    struct hash<ModelCacheKey> {
        size_t operator()(const ModelCacheKey& key) const {
            return std::hash<uint64_t>{}(key.meshHash);
        }
    };
}

/**
 * @brief Cached model data for Raylib Models
 */
struct CachedModelData {
    Model model;
    bool isStatic;
    uint64_t lastAccessFrame;
    bool isUnloaded;
    
    CachedModelData() : isStatic(false), lastAccessFrame(0), isUnloaded(false) {
        model = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f)); // Default empty model
    }
    
    ~CachedModelData() {
        UnloadSafely();
    }
    
    void UnloadSafely() {
        if (isUnloaded) return;
        
        // Clear the model struct to prevent double-free
        model.meshCount = 0;
        model.meshes = nullptr;
        model.materials = nullptr;
        model.materialCount = 0;
        model.bones = nullptr;
        model.boneCount = 0;
        model.bindPose = nullptr;
        
        isUnloaded = true;
    }
};

/**
 * @brief Model cache factory functions
 */
class ModelCacheFactory {
public:
    // Generate cache key from mesh component
    static ModelCacheKey GenerateKey(const MeshComponent& mesh);
    
    // Create model data from mesh component  
    static std::unique_ptr<CachedModelData> CreateModelData(const MeshComponent& mesh);
    
    // Calculate hash for mesh component
    static uint64_t CalculateMeshHash(const MeshComponent& mesh);
};

// Type alias for the complete model cache system
using ModelCache = CacheSystem<ModelCacheKey, CachedModelData, MeshComponent>;

//=============================================================================
// MATERIAL CACHING SPECIALIZATION  
//=============================================================================

/**
 * @brief Material cache key for Material flyweight pattern
 */
struct MaterialCacheKey {
    // Material properties for deduplication
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
    int materialType; // MaterialType enum as int
    std::string diffuseMap;
    std::string normalMap;
    std::string specularMap;
    std::string roughnessMap;
    std::string metallicMap;
    std::string aoMap;
    std::string emissiveMap;
    bool doubleSided;
    bool depthWrite;
    bool depthTest;
    bool castShadows;
    std::string materialName;
    
    bool operator==(const MaterialCacheKey& other) const {
        return primaryColor.r == other.primaryColor.r && primaryColor.g == other.primaryColor.g && 
               primaryColor.b == other.primaryColor.b && primaryColor.a == other.primaryColor.a &&
               secondaryColor.r == other.secondaryColor.r && secondaryColor.g == other.secondaryColor.g &&
               secondaryColor.b == other.secondaryColor.b && secondaryColor.a == other.secondaryColor.a &&
               specularColor.r == other.specularColor.r && specularColor.g == other.specularColor.g &&
               specularColor.b == other.specularColor.b && specularColor.a == other.specularColor.a &&
               shininess == other.shininess && alpha == other.alpha &&
               roughness == other.roughness && metallic == other.metallic && ao == other.ao &&
               emissiveColor.r == other.emissiveColor.r && emissiveColor.g == other.emissiveColor.g &&
               emissiveColor.b == other.emissiveColor.b && emissiveColor.a == other.emissiveColor.a &&
               emissiveIntensity == other.emissiveIntensity && materialType == other.materialType &&
               diffuseMap == other.diffuseMap && normalMap == other.normalMap && 
               specularMap == other.specularMap && roughnessMap == other.roughnessMap &&
               metallicMap == other.metallicMap && aoMap == other.aoMap && emissiveMap == other.emissiveMap &&
               doubleSided == other.doubleSided && depthWrite == other.depthWrite && 
               depthTest == other.depthTest && castShadows == other.castShadows &&
               materialName == other.materialName;
    }
};

// Hash function for MaterialCacheKey
namespace std {
    template <>
    struct hash<MaterialCacheKey> {
        size_t operator()(const MaterialCacheKey& key) const {
            size_t seed = 0;
            
            // Hash colors (convert to packed values)
            seed ^= hash<uint32_t>{}((key.primaryColor.r << 24) | (key.primaryColor.g << 16) | (key.primaryColor.b << 8) | key.primaryColor.a) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= hash<uint32_t>{}((key.secondaryColor.r << 24) | (key.secondaryColor.g << 16) | (key.secondaryColor.b << 8) | key.secondaryColor.a) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= hash<uint32_t>{}((key.specularColor.r << 24) | (key.specularColor.g << 16) | (key.specularColor.b << 8) | key.specularColor.a) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= hash<uint32_t>{}((key.emissiveColor.r << 24) | (key.emissiveColor.g << 16) | (key.emissiveColor.b << 8) | key.emissiveColor.a) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            
            // Hash floats
            seed ^= hash<float>{}(key.shininess) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= hash<float>{}(key.alpha) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= hash<float>{}(key.roughness) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= hash<float>{}(key.metallic) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= hash<float>{}(key.ao) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= hash<float>{}(key.emissiveIntensity) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            
            // Hash int and bool flags
            seed ^= hash<int>{}(key.materialType) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            uint32_t flags = (key.doubleSided ? 1 : 0) | ((key.depthWrite ? 1 : 0) << 1) | 
                           ((key.depthTest ? 1 : 0) << 2) | ((key.castShadows ? 1 : 0) << 3);
            seed ^= hash<uint32_t>{}(flags) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            
            // Hash strings
            seed ^= hash<string>{}(key.diffuseMap) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= hash<string>{}(key.normalMap) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= hash<string>{}(key.specularMap) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= hash<string>{}(key.roughnessMap) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= hash<string>{}(key.metallicMap) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= hash<string>{}(key.aoMap) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= hash<string>{}(key.emissiveMap) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= hash<string>{}(key.materialName) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            
            return seed;
        }
    };
}

/**
 * @brief Material data - what gets cached (flyweight intrinsic state)
 */
struct CachedMaterialData {
    // Basic material properties
    Color primaryColor = WHITE;      
    Color secondaryColor = BLACK;    
    Color specularColor = WHITE;
    float shininess = 32.0f;
    float alpha = 1.0f;

    // PBR properties
    float roughness = 0.5f;
    float metallic = 0.0f;
    float ao = 1.0f;

    // Emission properties
    Color emissiveColor = BLACK;
    float emissiveIntensity = 1.0f;

    // Material type
    enum class MaterialType {
        BASIC,      // Standard Blinn-Phong shading
        PBR,        // Physically-based rendering
        UNLIT,      // Unlit, no lighting calculations
        EMISSIVE,   // Self-illuminated
        TRANSPARENT // Alpha-blended transparency
    };

    MaterialType type = MaterialType::BASIC;

    // Texture maps (resolved through AssetSystem)
    std::string diffuseMap;
    std::string normalMap;
    std::string specularMap;
    std::string roughnessMap;
    std::string metallicMap;
    std::string aoMap;
    std::string emissiveMap;

    // Rendering flags
    bool doubleSided = false;
    bool depthWrite = true;
    bool depthTest = true;
    bool castShadows = true;

    // Material metadata
    std::string materialName = "default";
};

/**
 * @brief Material properties - what gets passed in to create materials
 */
struct MaterialProperties {
    // Copy all the same fields as CachedMaterialData for now
    // This could be optimized later to only include the essential creation properties
    Color primaryColor = WHITE;      
    Color secondaryColor = BLACK;    
    Color specularColor = WHITE;
    float shininess = 32.0f;
    float alpha = 1.0f;
    float roughness = 0.5f;
    float metallic = 0.0f;
    float ao = 1.0f;
    Color emissiveColor = BLACK;
    float emissiveIntensity = 1.0f;
    CachedMaterialData::MaterialType type = CachedMaterialData::MaterialType::BASIC;
    std::string diffuseMap;
    std::string normalMap;
    std::string specularMap;
    std::string roughnessMap;
    std::string metallicMap;
    std::string aoMap;
    std::string emissiveMap;
    bool doubleSided = false;
    bool depthWrite = true;
    bool depthTest = true;
    bool castShadows = true;
    std::string materialName = "default";
};

/**
 * @brief Material cache factory functions
 */
class MaterialCacheFactory {
public:
    // Generate cache key from material properties
    static MaterialCacheKey GenerateKey(const MaterialProperties& props);
    
    // Create material data from properties  
    static std::unique_ptr<CachedMaterialData> CreateMaterialData(const MaterialProperties& props);
};

// Type alias for the complete material cache system
using MaterialCache = CacheSystem<MaterialCacheKey, CachedMaterialData, MaterialProperties>;

//=============================================================================
// LIGHT CACHING SPECIALIZATION
//=============================================================================

// Forward declaration
struct LightComponent;
enum class LightType;

/**
 * @brief Light cache key for deduplication
 */
struct LightCacheKey {
    LightType type;
    Color color;
    float intensity;
    float radius;  // For point lights
    float range;   // For spot lights  
    float innerAngle, outerAngle; // For spot lights
    bool castShadows;
    
    bool operator==(const LightCacheKey& other) const;
};

/**
 * @brief Raylib-style light data for shader communication
 */
struct RaylibLight {
    int type;           // LIGHT_DIRECTIONAL = 0, LIGHT_POINT = 1, LIGHT_SPOT = 2
    int enabled;        // Light enabled flag (matches shader expectation)
    Vector3 position;   // Light position
    Vector3 target;     // Light target (for directional/spot lights)
    float color[4];     // Light color (RGBA normalized)
    float attenuation;  // Light attenuation (matches Raylib rlights.h)
    
    // Shader uniform locations (cached for performance)
    int typeLoc = -1;
    int enabledLoc = -1;
    int positionLoc = -1;
    int targetLoc = -1;
    int colorLoc = -1;
    int attenuationLoc = -1;  // Updated from intensityLoc
};

/**
 * @brief Cached light data (flyweight pattern)
 */
struct CachedLightData {
    RaylibLight raylibLight;
    bool isDirty = true;  // Needs shader update
};

// Hash specialization for LightCacheKey
namespace std {
    template<>
    struct hash<LightCacheKey> {
        size_t operator()(const LightCacheKey& key) const;
    };
}

/**
 * @brief Light cache factory for creating cached light data
 */
class LightCacheFactory {
public:
    static LightCacheKey GenerateKey(const LightComponent& lightComp);
    static std::unique_ptr<CachedLightData> CreateLightData(const LightComponent& lightComp);
};

// Light cache type aliases
using LightCache = CacheSystem<LightCacheKey, CachedLightData, LightComponent>;
