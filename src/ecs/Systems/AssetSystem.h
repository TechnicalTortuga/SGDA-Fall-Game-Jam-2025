#pragma once

#include "../System.h"
#include "../../rendering/TextureManager.h"
#include <unordered_map>
#include <memory>
#include <string>
#include <filesystem>

/**
 * @brief Centralized Asset Management System
 * 
 * This system provides a high-level interface for managing game assets,
 * with a focus on texture and material management. It works with the
 * TextureManager to handle low-level resource loading and caching.
 * 
 * Features:
 * - Unified asset loading interface
 * - Reference counting for resources
 * - Automatic cleanup of unused assets
 * - Support for both direct and handle-based access
 */
class AssetSystem : public System {
public:
    AssetSystem();
    ~AssetSystem() override;

    // System interface
    void Initialize() override;
    void Update(float deltaTime) override;
    void Shutdown() override;
    const char* GetName() const { return "AssetSystem"; }

    // Texture Management
    // =================
    
    /**
     * @brief Get or load a texture (main interface - similar to RenderAssetCache)
     * 
     * This is the primary method for getting textures. It will load the texture
     * if not already cached, or return the cached version if available.
     * 
     * @param path Path to the texture file (relative to asset root)
     * @return Pointer to the texture, or nullptr if loading failed
     */
    Texture2D* GetOrLoadTexture(const std::string& path);
    
    /**
     * @brief Load a texture from the specified path
     * 
     * @param path Path to the texture file (relative to asset root)
     * @return true if loaded successfully, false otherwise
     */
    bool LoadTexture(const std::string& path);
    
    /**
     * @brief Unload a texture by its path
     * 
     * @param path Path to the texture file
     * @return true if unloaded, false if not found or in use
     */
    bool UnloadTexture(const std::string& path);
    
    /**
     * @brief Get a texture by its path
     * 
     * @param path Path to the texture file
     * @return Texture2D The texture, or an empty texture if not found
     */
    Texture2D GetTexture(const std::string& path);
    
    /**
     * @brief Check if a texture is loaded
     * 
     * @param path Path to the texture file
     * @return true if the texture is loaded, false otherwise
     */
    bool HasTexture(const std::string& path) const;

    // Texture Handles
    // ==============
    
    /**
     * @brief Handle to a texture resource
     * 
     * Lightweight reference to a texture that can be safely passed around
     * and stored in components. The actual texture is managed by the AssetSystem.
     */
    struct TextureHandle {
        std::string path;      // Path to the texture
        bool isValid = false;  // Whether this handle is valid

        TextureHandle() = default;
        explicit TextureHandle(const std::string& path) : path(path), isValid(true) {}

        bool operator==(const TextureHandle& other) const {
            return path == other.path && isValid == other.isValid;
        }

        bool operator!=(const TextureHandle& other) const {
            return !(*this == other);
        }

        /**
         * @brief Check if this handle is valid
         * 
         * @return true if the handle is valid, false otherwise
         */
        explicit operator bool() const { return isValid && !path.empty(); }
        
        /**
         * @brief Invalidate this handle
         */
        void Invalidate() { isValid = false; path.clear(); }
    };

    /**
     * @brief Get a texture handle for the specified path
     * 
     * @param path Path to the texture file
     * @return TextureHandle A handle to the texture
     */
    TextureHandle GetTextureHandle(const std::string& path);
    
    /**
     * @brief Get a texture from a handle
     * 
     * @param handle The texture handle
     * @return Pointer to the texture, or nullptr if the handle is invalid
     */
    Texture2D* GetTexture(const TextureHandle& handle);

    // Asset Management & Cache Statistics
    // ===================================
    
    /**
     * @brief Cache statistics (similar to RenderAssetCache)
     */
    struct CacheStats {
        size_t totalRequests = 0;
        size_t cacheHits = 0;
        size_t cacheMisses = 0;
        size_t loadedTextures = 0;
        size_t totalMemoryBytes = 0;
        
        float GetHitRate() const { 
            return totalRequests > 0 ? (float)cacheHits / totalRequests : 0.0f; 
        }
    };
    
    /**
     * @brief Get cache statistics
     */
    const CacheStats& GetCacheStats() const { return cacheStats_; }
    
    /**
     * @brief Reset cache statistics
     */
    void ResetCacheStats();
    
    /**
     * @brief Get the number of loaded textures
     */
    size_t GetLoadedTextureCount() const;
    
    /**
     * @brief Get the total memory used by all loaded textures (in bytes)
     */
    size_t GetTotalTextureMemory() const;
    
    /**
     * @brief Clean up unused textures
     * 
     * Unloads any textures that are no longer referenced
     */
    void CleanupUnusedTextures();
    
    /**
     * @brief Force unload all textures
     * 
     * Warning: This will unload all textures, even if they're still in use
     */
    void ForceUnloadAllTextures();

    // Path Utilities
    // =============
    
    /**
     * @brief Resolve a relative asset path to a full path
     * 
     * @param relativePath Relative path to resolve
     * @return std::string Full path to the asset
     */
    std::string GetAssetPath(const std::string& relativePath) const;
    
    /**
     * @brief Set the root directory for assets
     * 
     * @param path Path to the asset root directory
     */
    void SetAssetRoot(const std::string& path);

private:
    // Reference counting for textures
    struct TextureRef {
        size_t refCount = 0;
        bool persistent = false; // If true, won't be unloaded by CleanupUnusedTextures()
        uint64_t lastAccessFrame = 0; // For LRU eviction if needed
    };
    
    std::unordered_map<std::string, TextureRef> textureRefs_;
    std::string assetRootPath_;
    bool initialized_ = false;
    
    // Cache statistics
    mutable CacheStats cacheStats_;
    uint64_t currentFrame_ = 0;

    // Helper methods
    bool IsValidTexturePath(const std::string& path) const;
    std::string NormalizeAssetPath(const std::string& path) const;
    size_t EstimateTextureMemory(const Texture2D& texture) const;
    void UpdateCacheStats(bool hit) const;
    void UpdateAccessTime(const std::string& path);
};
