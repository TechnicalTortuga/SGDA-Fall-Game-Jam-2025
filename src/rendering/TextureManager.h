#pragma once
#include "raylib.h"
#include <string>
#include <unordered_map>

// Simple texture cache for executable-relative asset loading
class TextureManager {
public:
    static TextureManager& Get() {
        static TextureManager instance;
        return instance;
    }

    // Load a texture from a path relative to the executable (e.g., "assets/textures/foo.png")
    // Returns a cached Texture2D. If load fails, returns {0}.
    Texture2D Load(const std::string& relativePath);

    // Get texture if already loaded; returns {0} if not present
    Texture2D GetTexture(const std::string& relativePath) const;

    // Unload all cached textures
    void UnloadAll();

private:
    TextureManager() = default;
    ~TextureManager() = default;
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

    std::unordered_map<std::string, Texture2D> cache_;
};
