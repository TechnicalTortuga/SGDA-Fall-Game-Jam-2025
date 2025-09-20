#include "AssetSystem.h"
#include "../../utils/Logger.h"
#include <algorithm>

AssetSystem::AssetSystem()
    : assetRootPath_("assets/textures/"), initialized_(false) {
    LOG_INFO("AssetSystem created");
}

AssetSystem::~AssetSystem() {
    if (initialized_) {
        Shutdown();
    }
    LOG_INFO("AssetSystem destroyed");
}

void AssetSystem::Initialize() {
    if (initialized_) {
        LOG_WARNING("AssetSystem already initialized");
        return;
    }

    LOG_INFO("Initializing AssetSystem");

    // Set up component signature (if needed for future asset components)
    // For now, AssetSystem doesn't require specific components

    // Ensure asset directory exists
    // Note: In a real game, you'd want to create directories if they don't exist

    initialized_ = true;
    LOG_INFO("AssetSystem initialized successfully");
}

void AssetSystem::Update(float deltaTime) {
    // Asset system doesn't need per-frame updates for now
    // Could be used for async loading progress, memory management, etc.
}

void AssetSystem::Shutdown() {
    if (!initialized_) return;

    LOG_INFO("Shutting down AssetSystem");

    // Unload all textures
    ForceUnloadAllTextures();

    initialized_ = false;
    LOG_INFO("AssetSystem shutdown completed");
}

bool AssetSystem::LoadTexture(const std::string& path, const std::string& name) {
    if (!initialized_) {
        LOG_ERROR("AssetSystem::LoadTexture - System not initialized");
        return false;
    }

    // Check if texture already loaded
    if (HasTexture(name)) {
        LOG_DEBUG("AssetSystem::LoadTexture - Texture '" + name + "' already loaded, incrementing reference count");
        referenceCounts_[name]++;
        return true;
    }

    // Construct full path
    std::string fullPath = GetAssetPath(path);

    // Validate path
    if (!IsValidTexturePath(fullPath)) {
        LOG_ERROR("AssetSystem::LoadTexture - Invalid texture path: " + fullPath);
        return false;
    }

    // Load texture using Raylib
    Texture2D texture = ::LoadTexture(fullPath.c_str());

    if (texture.id == 0) {
        LOG_ERROR("AssetSystem::LoadTexture - Failed to load texture: " + fullPath);
        return false;
    }

    // Store texture
    textures_[name] = texture;
    referenceCounts_[name] = 1;

    LOG_INFO("AssetSystem::LoadTexture - Successfully loaded texture '" + name + "' from " + fullPath +
             " (" + std::to_string(texture.width) + "x" + std::to_string(texture.height) + ")");

    return true;
}

bool AssetSystem::UnloadTexture(const std::string& name) {
    if (!initialized_) {
        LOG_ERROR("AssetSystem::UnloadTexture - System not initialized");
        return false;
    }

    auto it = textures_.find(name);
    if (it == textures_.end()) {
        LOG_WARNING("AssetSystem::UnloadTexture - Texture '" + name + "' not found");
        return false;
    }

    // Decrement reference count
    if (--referenceCounts_[name] <= 0) {
        // Unload texture using Raylib
        ::UnloadTexture(it->second);
        textures_.erase(it);
        referenceCounts_.erase(name);

        LOG_INFO("AssetSystem::UnloadTexture - Unloaded texture '" + name + "'");
    } else {
        LOG_DEBUG("AssetSystem::UnloadTexture - Texture '" + name + "' reference count decreased to " +
                  std::to_string(referenceCounts_[name]));
    }

    return true;
}

Texture2D* AssetSystem::GetTexture(const std::string& name) {
    auto it = textures_.find(name);
    return (it != textures_.end()) ? &it->second : nullptr;
}

bool AssetSystem::HasTexture(const std::string& name) const {
    return textures_.find(name) != textures_.end();
}

AssetSystem::TextureHandle AssetSystem::GetTextureHandle(const std::string& name) {
    if (HasTexture(name)) {
        return TextureHandle(name);
    }
    return TextureHandle(); // Invalid handle
}

Texture2D* AssetSystem::GetTexture(const TextureHandle& handle) {
    if (!handle.isValid) {
        return nullptr;
    }
    return GetTexture(handle.assetName);
}

size_t AssetSystem::GetLoadedTextureCount() const {
    return textures_.size();
}

size_t AssetSystem::GetTotalTextureMemory() const {
    size_t totalMemory = 0;
    for (const auto& pair : textures_) {
        totalMemory += EstimateTextureMemory(pair.second);
    }
    return totalMemory;
}

void AssetSystem::CleanupUnusedTextures() {
    if (!initialized_) return;

    LOG_INFO("AssetSystem::CleanupUnusedTextures - Starting cleanup");

    std::vector<std::string> toUnload;

    for (const auto& pair : referenceCounts_) {
        if (pair.second <= 0) {
            toUnload.push_back(pair.first);
        }
    }

    for (const auto& name : toUnload) {
        auto it = textures_.find(name);
        if (it != textures_.end()) {
            ::UnloadTexture(it->second);
            textures_.erase(it);
            referenceCounts_.erase(name);
            LOG_INFO("AssetSystem::CleanupUnusedTextures - Cleaned up texture '" + name + "'");
        }
    }

    if (!toUnload.empty()) {
        LOG_INFO("AssetSystem::CleanupUnusedTextures - Cleaned up " + std::to_string(toUnload.size()) + " unused textures");
    } else {
        LOG_DEBUG("AssetSystem::CleanupUnusedTextures - No unused textures to clean up");
    }
}

void AssetSystem::ForceUnloadAllTextures() {
    if (!initialized_) return;

    LOG_INFO("AssetSystem::ForceUnloadAllTextures - Unloading all " + std::to_string(textures_.size()) + " textures");

    for (auto& pair : textures_) {
        ::UnloadTexture(pair.second);
    }

    textures_.clear();
    referenceCounts_.clear();

    LOG_INFO("AssetSystem::ForceUnloadAllTextures - All textures unloaded");
}

std::string AssetSystem::GetAssetPath(const std::string& relativePath) const {
    return assetRootPath_ + relativePath;
}

bool AssetSystem::IsValidTexturePath(const std::string& path) const {
    // Basic validation - check if path ends with common texture extensions
    std::string lowerPath = path;
    std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);

    // Check file extensions using substr for C++ compatibility
    if (lowerPath.length() >= 4) {
        std::string ext = lowerPath.substr(lowerPath.length() - 4);
        if (ext == ".png" || ext == ".jpg" || ext == ".bmp" || ext == ".tga") return true;
    }
    if (lowerPath.length() >= 5) {
        std::string ext = lowerPath.substr(lowerPath.length() - 5);
        if (ext == ".jpeg") return true;
    }

    return false;
}

std::string AssetSystem::NormalizeAssetPath(const std::string& path) const {
    // Remove any leading slashes and ensure consistent path format
    std::string normalized = path;

    // Remove leading slashes
    while (!normalized.empty() && normalized[0] == '/') {
        normalized = normalized.substr(1);
    }

    return normalized;
}

size_t AssetSystem::EstimateTextureMemory(const Texture2D& texture) const {
    // Rough estimation: width * height * bytes per pixel
    // Raylib typically uses RGBA (4 bytes per pixel)
    return static_cast<size_t>(texture.width) * texture.height * 4;
}

// TextureHandle implementation
Texture2D* AssetSystem::TextureHandle::GetTexture() const {
    if (!IsValid()) {
        return nullptr;
    }

    // Get AssetSystem instance - this assumes AssetSystem is accessible
    // In practice, this would be called from a System that has access to Engine
    // For now, return nullptr - will be implemented when integrated with Engine
    LOG_WARNING("TextureHandle::GetTexture() called but AssetSystem access not implemented");
    return nullptr;
}
