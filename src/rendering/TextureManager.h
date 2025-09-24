#pragma once
#include "raylib.h"
#include <string>
#include <unordered_map>
#include <memory>
#include "../utils/Logger.h"

// Manages texture loading, caching, and reference counting.
// This is a singleton class that handles all texture resources in the game.
// It provides reference counting to ensure textures are only loaded once
// and unloaded when no longer in use.
class TextureManager {
public:
    // Get the singleton instance of TextureManager
    static TextureManager& Get() {
        static TextureManager instance;
        return instance;
    }

    // Load a texture from the given path. The path can be relative to the executable
    // or an absolute path. Returns an empty texture on failure.
    Texture2D Load(const std::string& path);
    
    // Get a texture if it's already loaded. Returns an empty texture if not found.
    Texture2D GetTexture(const std::string& path) const;
    
    // Get a pointer to a texture if it's already loaded. Returns nullptr if not found.
    // This is useful for AssetSystem to avoid copying textures.
    const Texture2D* GetTexturePtr(const std::string& path) const;
    
    
    // Unload a specific texture. Returns true if unloaded, false if not found.
    bool Unload(const std::string& path);
    
    // Unload all textures currently managed by this TextureManager.
    void UnloadAll();
    
    // Check if a texture is currently loaded in the cache.
    bool IsLoaded(const std::string& path) const;

private:
    // Private constructor for singleton pattern
    TextureManager() = default;
    
    // Destructor ensures all textures are properly unloaded
    ~TextureManager() { UnloadAll(); }
    
    // Prevent copying
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

    // Internal structure to track texture references and data
    struct TextureEntry {
        Texture2D texture;
        std::string path;
        int refCount = 0;
        
        // Initialize with an invalid texture
        TextureEntry() : texture{0,0,1,1,7} {}
        
        // Automatically unload the texture when destroyed
        ~TextureEntry() {
            if (texture.id != 0) {
                UnloadTexture(texture);
                LOG_DEBUG("Unloaded texture: " + path);
            }
        }
    };
    
    // Cache of all loaded textures, keyed by their full path
    std::unordered_map<std::string, std::shared_ptr<TextureEntry>> cache_;
};
