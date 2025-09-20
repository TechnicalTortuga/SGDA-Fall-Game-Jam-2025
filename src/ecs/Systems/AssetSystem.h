#pragma once

#include "../System.h"
#include "../../rendering/TextureManager.h"
#include <unordered_map>
#include <memory>
#include <string>

/*
AssetSystem - Centralized Resource Management for ECS

This system manages all game assets (textures, materials, meshes, etc.)
providing centralized loading, caching, and lifecycle management. It works
with TextureComponent and MaterialComponent to provide lightweight resource
references without direct resource storage.

Features:
- Texture loading and caching
- Reference counting for resource management
- Asset lifecycle management
- Integration with ECS components
- Memory optimization through resource sharing
*/

class AssetSystem : public System {
public:
    AssetSystem();
    ~AssetSystem();

    // System interface
    void Initialize() override;
    void Update(float deltaTime) override;
    void Shutdown() override;
    const char* GetName() const { return "AssetSystem"; }

    // Texture management
    bool LoadTexture(const std::string& path, const std::string& name);
    bool UnloadTexture(const std::string& name);
    Texture2D* GetTexture(const std::string& name);
    bool HasTexture(const std::string& name) const;

    // Texture resource handles (for component integration)
    struct TextureHandle {
        std::string assetName;
        bool isValid = false;

        TextureHandle() = default;
        TextureHandle(const std::string& name) : assetName(name), isValid(true) {}

        bool operator==(const TextureHandle& other) const {
            return assetName == other.assetName;
        }

        bool operator!=(const TextureHandle& other) const {
            return !(*this == other);
        }

        // Handle validity and lifecycle
        bool IsValid() const { return isValid && !assetName.empty(); }
        void Invalidate() { isValid = false; assetName.clear(); }

        // Get the texture resource (safe access through AssetSystem)
        Texture2D* GetTexture() const;
    };

    // Handle-based texture access (preferred for components)
    TextureHandle GetTextureHandle(const std::string& name);
    Texture2D* GetTexture(const TextureHandle& handle);

    // Asset statistics
    size_t GetLoadedTextureCount() const;
    size_t GetTotalTextureMemory() const; // Approximate

    // Asset cleanup
    void CleanupUnusedTextures();
    void ForceUnloadAllTextures();

    // Path utilities
    std::string GetAssetPath(const std::string& relativePath) const;

private:
    // Texture storage
    std::unordered_map<std::string, Texture2D> textures_;
    std::unordered_map<std::string, int> referenceCounts_;

    // Asset paths
    std::string assetRootPath_;

    // Helper methods
    bool IsValidTexturePath(const std::string& path) const;
    std::string NormalizeAssetPath(const std::string& path) const;
    size_t EstimateTextureMemory(const Texture2D& texture) const;

    // System state
    bool initialized_;
};
