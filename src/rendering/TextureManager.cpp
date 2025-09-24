#include "TextureManager.h"
#include "../utils/PathUtils.h"
#include "../utils/Logger.h"
#include <filesystem>

namespace fs = std::filesystem;

using namespace std;

Texture2D TextureManager::Load(const string& path) {
    // Convert path to absolute and normalize it
    string absPath = fs::absolute(path).lexically_normal().string();
    
    // Check if already loaded
    auto it = cache_.find(absPath);
    if (it != cache_.end()) {
        it->second->refCount++;
        LOG_DEBUG("Reusing cached texture: " + absPath + " (refs: " + to_string(it->second->refCount) + ")");
        return it->second->texture;
    }
    
    // Try multiple paths if the first attempt fails
    string exeDir = Utils::GetExecutableDir();
    vector<string> attempts = {
        absPath,                                    // Absolute path first
        fs::path(exeDir) / path,                   // Relative to executable
        path,                                      // CWD-relative
        fs::path(exeDir).parent_path().parent_path() / path // Project root relative
    };
    
    Texture2D texture = {0};
    
    for (const auto& attempt : attempts) {
        LOG_DEBUG("Attempting to load texture: " + attempt);
        texture = LoadTexture(attempt.c_str());
        if (texture.id != 0) {
            absPath = fs::absolute(attempt).lexically_normal().string();
            break;
        }
    }
    
    if (texture.id == 0) {
        LOG_ERROR("Failed to load texture from all paths: " + path);
        return {}; // Return empty texture
    }
    
    // Create new entry
    auto entry = make_shared<TextureEntry>();
    entry->texture = texture;
    entry->path = absPath;
    entry->refCount = 1;
    
    // Add to cache
    cache_[absPath] = entry;
    
    // Set default texture parameters
    SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(texture, TEXTURE_WRAP_REPEAT);
    
    LOG_DEBUG("Loaded texture: " + absPath + " (ID: " + to_string(texture.id) + 
              ", Size: " + to_string(texture.width) + "x" + to_string(texture.height) + ")");
    
    return texture;
}

Texture2D TextureManager::GetTexture(const string& path) const {
    string absPath = fs::absolute(path).lexically_normal().string();
    auto it = cache_.find(absPath);
    if (it != cache_.end() && it->second->texture.id != 0) {
        return it->second->texture;
    }
    return {}; // Return empty texture
}

const Texture2D* TextureManager::GetTexturePtr(const string& path) const {
    string absPath = fs::absolute(path).lexically_normal().string();
    auto it = cache_.find(absPath);
    if (it != cache_.end() && it->second->texture.id != 0) {
        return &it->second->texture;
    }
    return nullptr; // Return null pointer
}


bool TextureManager::Unload(const string& path) {
    string absPath = fs::absolute(path).lexically_normal().string();
    auto it = cache_.find(absPath);
    
    if (it != cache_.end()) {
        it->second->refCount--;
        LOG_DEBUG("Decremented ref count for texture: " + absPath + " (refs: " + to_string(it->second->refCount) + ")");
        
        if (it->second->refCount <= 0) {
            LOG_DEBUG("Unloading texture: " + absPath);
            cache_.erase(it);
            return true;
        }
        return true; // Still in use by others
    }
    
    LOG_WARNING("Attempted to unload non-existent texture: " + absPath);
    return false;
}

void TextureManager::UnloadAll() {
    LOG_DEBUG("Unloading all textures (" + to_string(cache_.size()) + " textures)");
    cache_.clear(); // Shared pointers will handle cleanup
}

bool TextureManager::IsLoaded(const string& path) const {
    string absPath = fs::absolute(path).lexically_normal().string();
    auto it = cache_.find(absPath);
    return it != cache_.end() && it->second->texture.id != 0;
}
