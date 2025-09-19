#include "TextureManager.h"
#include "../utils/PathUtils.h"
#include "../utils/Logger.h"

Texture2D TextureManager::Load(const std::string& relativePath) {
    // Return cached if present
    auto it = cache_.find(relativePath);
    if (it != cache_.end()) return it->second;

    std::string exeDir = Utils::GetExecutableDir();
    std::string attempt1 = exeDir + "/" + relativePath;            // build/bin/assets/...
    std::string attempt2 = relativePath;                             // CWD-relative
    std::string attempt3 = exeDir + "/../../" + relativePath;       // project-root relative when exeDir is build/bin

    LOG_INFO(std::string("TextureManager: Loading ") + attempt1);
    Texture2D tex = LoadTexture(attempt1.c_str());
    if (tex.id == 0) {
        LOG_WARNING(std::string("TextureManager: Failed ") + attempt1 + ", trying " + attempt2);
        tex = LoadTexture(attempt2.c_str());
    }
    if (tex.id == 0) {
        LOG_WARNING(std::string("TextureManager: Failed ") + attempt2 + ", trying " + attempt3);
        tex = LoadTexture(attempt3.c_str());
    }
    if (tex.id == 0) {
        LOG_ERROR(std::string("TextureManager: Failed to load ") + relativePath + " from all attempts");
        return (Texture2D){0};
    }
    // Improve default sampling
    SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(tex, TEXTURE_WRAP_CLAMP);

    cache_[relativePath] = tex;
    return tex;
}

Texture2D TextureManager::GetTexture(const std::string& relativePath) const {
    auto it = cache_.find(relativePath);
    if (it != cache_.end()) return it->second;
    return (Texture2D){0};
}

void TextureManager::UnloadAll() {
    for (auto& kv : cache_) {
        if (kv.second.id != 0) UnloadTexture(kv.second);
    }
    cache_.clear();
}
