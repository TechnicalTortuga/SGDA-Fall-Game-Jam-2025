#include "AssetSystem.h"
#include "../../utils/Logger.h"
#include "../../utils/PathUtils.h"
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

AssetSystem::AssetSystem() {
    // Set asset root to the assets directory next to the executable
    std::string exeDir = Utils::GetExecutableDir();
    assetRootPath_ = fs::absolute(fs::path(exeDir) / "assets").string();
    LOG_INFO("AssetSystem created with asset root: " + assetRootPath_);
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
    
    // Create asset directories if they don't exist
    try {
        fs::create_directories(assetRootPath_);
        fs::create_directories(assetRootPath_ + "/textures");
        fs::create_directories(assetRootPath_ + "/materials");
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to create asset directories: " + std::string(e.what()));
        return;
    }

    initialized_ = true;
    LOG_INFO("AssetSystem initialized successfully");
}

void AssetSystem::Update(float /*deltaTime*/) {
    // Update frame counter for cache statistics
    currentFrame_++;
    
    // Could be used for async loading, memory management, etc.
}

void AssetSystem::Shutdown() {
    if (!initialized_) return;

    LOG_INFO("Shutting down AssetSystem");
    
    // Clean up all textures
    ForceUnloadAllTextures();
    
    initialized_ = false;
    LOG_INFO("AssetSystem shutdown completed");
}

Texture2D* AssetSystem::GetOrLoadTexture(const std::string& path) {
    if (!initialized_) {
        LOG_ERROR("Cannot get/load texture - AssetSystem not initialized");
        UpdateCacheStats(false);
        return nullptr;
    }
    
    std::string absPath = GetAssetPath(path);
    
    // Check if already loaded (cache hit)
    if (HasTexture(absPath)) {
        UpdateAccessTime(absPath);
        UpdateCacheStats(true);
        
        auto& textureManager = TextureManager::Get();
        const Texture2D* texture = textureManager.GetTexturePtr(absPath);
        
        if (texture && texture->id != 0) {
            LOG_DEBUG("Cache hit for texture: " + absPath);
            return const_cast<Texture2D*>(texture); // Return pointer to cached texture
        }
    }
    
    // Cache miss - try to load the texture
    UpdateCacheStats(false);
    
    auto& textureManager = TextureManager::Get();
    Texture2D texture = textureManager.Load(absPath);
    
    if (texture.id == 0) {
        LOG_ERROR("Failed to load texture: " + absPath);
        return nullptr;
    }
    
    // Add to our reference tracking
    textureRefs_[absPath] = {1, false, currentFrame_}; // refCount=1, not persistent, current frame
    cacheStats_.loadedTextures++;
    cacheStats_.totalMemoryBytes += EstimateTextureMemory(texture);
    
    LOG_DEBUG("Loaded new texture: " + absPath + " (ID: " + std::to_string(texture.id) + 
              ", Size: " + std::to_string(texture.width) + "x" + std::to_string(texture.height) + ")");
    
    // Return pointer to the newly loaded texture
    const Texture2D* texturePtr = textureManager.GetTexturePtr(absPath);
    return texturePtr ? const_cast<Texture2D*>(texturePtr) : nullptr;
}

bool AssetSystem::LoadTexture(const std::string& path) {
    if (!initialized_) {
        LOG_ERROR("Cannot load texture - AssetSystem not initialized");
        return false;
    }
    
    std::string absPath = GetAssetPath(path);
    
    // Check if already loaded
    if (HasTexture(absPath)) {
        // Increment reference count
        textureRefs_[absPath].refCount++;
        LOG_DEBUG("Texture already loaded, increased ref count: " + absPath + " (refs: " + 
                 std::to_string(textureRefs_[absPath].refCount) + ")");
        return true;
    }
    
    // Try to load the texture
    auto& textureManager = TextureManager::Get();
    Texture2D texture = textureManager.Load(absPath);
    
    if (texture.id == 0) {
        LOG_ERROR("Failed to load texture: " + absPath);
        return false;
    }
    
    // Add to our reference tracking
    textureRefs_[absPath] = {1, false}; // refCount=1, not persistent
    
    LOG_DEBUG("Loaded texture: " + absPath + " (ID: " + std::to_string(texture.id) + 
              ", Size: " + std::to_string(texture.width) + "x" + std::to_string(texture.height) + ")");
    
    return true;
}

bool AssetSystem::UnloadTexture(const std::string& path) {
    if (!initialized_) {
        LOG_ERROR("Cannot unload texture - AssetSystem not initialized");
        return false;
    }
    
    std::string absPath = GetAssetPath(path);
    auto it = textureRefs_.find(absPath);
    
    if (it == textureRefs_.end()) {
        LOG_WARNING("Cannot unload texture - not found: " + absPath);
        return false;
    }
    
    // Decrement reference count
    it->second.refCount--;
    
    if (it->second.refCount <= 0) {
        // No more references, unload the texture
        LOG_DEBUG("Unloading texture (no more references): " + absPath);
        TextureManager::Get().Unload(absPath);
        textureRefs_.erase(it);
    } else {
        LOG_DEBUG("Decremented ref count for texture: " + absPath + 
                " (refs: " + std::to_string(it->second.refCount) + ")");
    }
    
    return true;
}

Texture2D AssetSystem::GetTexture(const std::string& path) {
    std::string absPath = GetAssetPath(path);
    if (absPath.empty()) {
        LOG_WARNING("Cannot get texture with empty path");
        return Texture2D{0, 0, 1, 1, 7}; // Default to a 1x1 white pixel
    }

    // Check if texture is already loaded
    auto it = textureRefs_.find(absPath);
    if (it != textureRefs_.end()) {
        // Return the texture from the manager
        return TextureManager::Get().GetTexture(absPath);
    }

    // Try to load the texture
    if (LoadTexture(absPath)) {
        return TextureManager::Get().GetTexture(absPath);
    }

    return Texture2D{0, 0, 1, 1, 7}; // Return default texture
}

bool AssetSystem::HasTexture(const std::string& path) const {
    std::string absPath = GetAssetPath(path);
    if (absPath.empty()) {
        return false;
    }
    return textureRefs_.find(absPath) != textureRefs_.end() && 
           TextureManager::Get().IsLoaded(absPath);
    
    return false;
}

AssetSystem::TextureHandle AssetSystem::GetTextureHandle(const std::string& path) {
    if (!initialized_) {
        LOG_ERROR("Cannot get texture handle - AssetSystem not initialized");
        return {};
    }
    
    std::string absPath = GetAssetPath(path);
    
    // Try to load the texture if not already loaded
    if (!HasTexture(absPath) && !LoadTexture(absPath)) {
        LOG_ERROR("Failed to load texture for handle: " + absPath);
        return {}; // Return invalid handle
    }
    
    // Make sure the texture exists in our ref count map
    if (textureRefs_.find(absPath) == textureRefs_.end()) {
        LOG_ERROR("Texture not found in ref count map: " + absPath);
        return {}; // Return invalid handle
    }
    
    // Increment reference count
    textureRefs_[absPath].refCount++;
    
    LOG_DEBUG("Created texture handle for: " + absPath + 
             " (refs: " + std::to_string(textureRefs_[absPath].refCount) + ")");
    
    // Create and return a valid handle
    TextureHandle handle(absPath);
    handle.isValid = true;
    return handle;
}

Texture2D* AssetSystem::GetTexture(const TextureHandle& handle) {
    static Texture2D defaultTexture{0, 0, 1, 1, 7};

    if (!initialized_ || !handle.isValid) {
        LOG_WARNING("Invalid texture handle or AssetSystem not initialized");
        return &defaultTexture; // Return pointer to default texture
    }

    // Get the texture from the manager using the regular GetTexture method
    Texture2D texture = TextureManager::Get().GetTexture(handle.path);
    if (texture.id == 0) {
        LOG_ERROR("Texture handle references invalid texture: " + handle.path);
        return &defaultTexture; // Return pointer to default texture
    }

    // Store the texture in our cache and return a pointer to it
    // This ensures the texture lifetime is managed by AssetSystem
    auto& texManager = TextureManager::Get();
    std::string absPath = NormalizeAssetPath(handle.path);

    // We need to ensure this texture stays loaded, so we'll keep a reference
    // For now, return a pointer to a static copy (this is a temporary solution)
    static std::unordered_map<std::string, Texture2D> textureCache_;
    textureCache_[absPath] = texture;

    return &textureCache_[absPath];
}

size_t AssetSystem::GetLoadedTextureCount() const {
    if (!initialized_) return 0;
    return textureRefs_.size();
}

size_t AssetSystem::GetTotalTextureMemory() const {
    if (!initialized_) return 0;
    
    size_t total = 0;
    auto& textureManager = TextureManager::Get();
    
    for (const auto& pair : textureRefs_) {
        Texture2D tex = textureManager.GetTexture(pair.first);
        if (tex.id != 0) {
            total += EstimateTextureMemory(tex);
        }
    }
    
    return total;
}

void AssetSystem::CleanupUnusedTextures() {
    if (!initialized_) return;
    
    LOG_DEBUG("Cleaning up unused textures...");
    
    auto it = textureRefs_.begin();
    bool cleanedUp = false;
    while (it != textureRefs_.end()) {
        // Skip persistent textures and those still in use
        if (it->second.persistent || it->second.refCount > 0) {
            ++it;
            continue;
        }
        
        LOG_DEBUG("Unloading unused texture: " + it->first);
        TextureManager::Get().Unload(it->first);
        it = textureRefs_.erase(it);
        cleanedUp = true;
    }
    
    if (!cleanedUp) {
        LOG_DEBUG("No unused textures to clean up");
    }
}

void AssetSystem::ForceUnloadAllTextures() {
    if (!initialized_) return;
    
    LOG_INFO("Force unloading all textures (" + std::to_string(textureRefs_.size()) + " textures)");
    
    // Unload all textures through the texture manager
    for (auto& pair : textureRefs_) {
        if (TextureManager::Get().IsLoaded(pair.first)) {
            TextureManager::Get().Unload(pair.first);
        }
    }
    
    // Clear our reference tracking
    textureRefs_.clear();

    LOG_INFO("AssetSystem::ForceUnloadAllTextures - All textures unloaded");
}

std::string AssetSystem::GetAssetPath(const std::string& relativePath) const {
    // If the path is already absolute, return it as-is
    if (fs::path(relativePath).is_absolute()) {
        return fs::absolute(relativePath).lexically_normal().string();
    }
    
    // Otherwise, resolve it relative to the asset root
    return (fs::path(assetRootPath_) / relativePath).lexically_normal().string();
}

void AssetSystem::SetAssetRoot(const std::string& path) {
    std::string newPath = fs::absolute(path).lexically_normal().string();
    
    if (newPath != assetRootPath_) {
        LOG_INFO("Changing asset root from '" + assetRootPath_ + "' to '" + newPath + "'");
        assetRootPath_ = newPath;
        
        // Recreate asset directories
        try {
            fs::create_directories(assetRootPath_);
            fs::create_directories(assetRootPath_ + "/textures");
            fs::create_directories(assetRootPath_ + "/materials");
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to create asset directories: " + std::string(e.what()));
        }
    }
}

// Check if the given path has a valid texture file extension
// Returns true if the path has a valid texture extension
bool AssetSystem::IsValidTexturePath(const std::string& path) const {
    if (path.empty()) {
        return false;
    }
    
    // Common image extensions we support
    static const std::vector<std::string> validExtensions = {
        ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".gif", ".hdr", ".dds"
    };
    
    // Convert path to lowercase for case-insensitive comparison
    std::string lowerPath = path;
    std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), 
                  [](unsigned char c) { return std::tolower(c); });
    
    // Check if the path ends with any of the valid extensions
    for (const auto& ext : validExtensions) {
        if (lowerPath.length() >= ext.length() &&
            lowerPath.compare(lowerPath.length() - ext.length(), ext.length(), ext) == 0) {
            return true;
        }
    }
    
    return false;
}

std::string AssetSystem::NormalizeAssetPath(const std::string& path) const {
    if (path.empty()) {
        return path;
    }
    
    // Convert to forward slashes
    std::string result = path;
    std::replace(result.begin(), result.end(), '\\', '/');
    
    // Remove any leading ./ or .\ patterns
    if (result.size() > 1 && result[0] == '.' && (result[1] == '/' || result[1] == '\\')) {
        result = result.substr(2);
    }
    
    // Resolve any . or .. in the path
    try {
        return fs::path(result).lexically_normal().string();
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to normalize path '" + path + "': " + e.what());
        return path; // Fallback to original if path is invalid
    }
}

size_t AssetSystem::EstimateTextureMemory(const Texture2D& texture) const {
    if (texture.id == 0) return 0;
    
    // Very rough estimation - actual GPU memory usage may vary
    // This is just for informational purposes
    size_t bytesPerPixel = 4; // Assuming 32-bit RGBA by default
    
    // Check for HDR textures (assuming 16-bit per channel)
    if (texture.format == PIXELFORMAT_UNCOMPRESSED_R32 ||
        texture.format == PIXELFORMAT_UNCOMPRESSED_R32G32B32 ||
        texture.format == PIXELFORMAT_UNCOMPRESSED_R32G32B32A32) {
        bytesPerPixel = 16; // 4 channels * 4 bytes (32-bit float)
    }
    // Check for compressed textures (DXT, ETC, etc.)
    else if (texture.format >= PIXELFORMAT_COMPRESSED_DXT1_RGB && 
             texture.format <= PIXELFORMAT_COMPRESSED_ETC2_EAC_RGBA) {
        // For compressed textures, use a rough estimate of 4 bits per pixel
        // This is just an approximation as different compression formats vary
        return (texture.width * texture.height) / 2; // 4 bits per pixel = 0.5 bytes per pixel
    }
    
    return texture.width * texture.height * bytesPerPixel;
}

void AssetSystem::UpdateCacheStats(bool hit) const {
    cacheStats_.totalRequests++;
    if (hit) {
        cacheStats_.cacheHits++;
    } else {
        cacheStats_.cacheMisses++;
    }
}

void AssetSystem::UpdateAccessTime(const std::string& path) {
    auto it = textureRefs_.find(path);
    if (it != textureRefs_.end()) {
        it->second.lastAccessFrame = currentFrame_;
    }
}

void AssetSystem::ResetCacheStats() {
    cacheStats_ = CacheStats{};
    LOG_INFO("AssetSystem cache statistics reset");
}
