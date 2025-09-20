#include "Sprite.h"
#include "utils/Logger.h"
#include "rendering/TextureManager.h"

Sprite::Sprite()
    : texture_{0, 0, 0, 0, 0}, textureLoaded_(false), scale_(1.0f), rotation_(0.0f),
      tint_(WHITE), lightIntensity_(1.0f), decalOverlay_{0, 0, 0, 0, 0}
{
}

Sprite::Sprite(const std::string& texturePath)
    : texture_{0, 0, 0, 0, 0}, texturePath_(texturePath), textureLoaded_(false),
      scale_(1.0f), rotation_(0.0f), tint_(WHITE), lightIntensity_(1.0f), decalOverlay_{0, 0, 0, 0, 0}
{
    LoadTexture(texturePath);
}

Sprite::Sprite(const std::string& texturePath, float scale, float rotation)
    : texture_{0, 0, 0, 0, 0}, texturePath_(texturePath), textureLoaded_(false),
      scale_(scale), rotation_(rotation), tint_(WHITE), lightIntensity_(1.0f), decalOverlay_{0, 0, 0, 0, 0}
{
    LoadTexture(texturePath);
}

Sprite::~Sprite()
{
    UnloadTexture();
    ClearDecalOverlay();
}

bool Sprite::LoadTexture(const std::string& texturePath)
{
    if (textureLoaded_ && texturePath == texturePath_) {
        return true; // Already loaded
    }

    // Unload previous texture if loaded
    if (textureLoaded_) {
        UnloadTexture();
    }

    // Use TextureManager for consistent path resolution
    texture_ = TextureManager::Get().Load(texturePath);
    if (texture_.id == 0) {
        LOG_ERROR("Failed to load texture: " + texturePath);
        textureLoaded_ = false;
        return false;
    }

    texturePath_ = texturePath;
    textureLoaded_ = true;
    LOG_DEBUG("Loaded texture: " + texturePath);
    return true;
}

void Sprite::UnloadTexture()
{
    if (textureLoaded_) {
        ::UnloadTexture(texture_);
        texture_ = {0, 0, 0, 0, 0};
        textureLoaded_ = false;
        LOG_DEBUG("Unloaded texture: " + texturePath_);
    }
}

void Sprite::ClearDecalOverlay()
{
    if (decalOverlay_.id != 0) {
        ::UnloadTexture(decalOverlay_);
        decalOverlay_ = {0, 0, 0, 0, 0};
    }
}

Rectangle Sprite::GetSourceRect() const
{
    if (!textureLoaded_) {
        return {0, 0, 0, 0};
    }

    return {0, 0, (float)texture_.width, (float)texture_.height};
}

Vector2 Sprite::GetOrigin() const
{
    if (!textureLoaded_) {
        return {0, 0};
    }

    return {(float)texture_.width * 0.5f, (float)texture_.height * 0.5f};
}

float Sprite::GetWidth() const
{
    return textureLoaded_ ? texture_.width * scale_ : 0.0f;
}

float Sprite::GetHeight() const
{
    return textureLoaded_ ? texture_.height * scale_ : 0.0f;
}

// Removed OnAttach/OnDetach methods - ECS components should not have lifecycle knowledge
// Texture unloading should be handled by AssetSystem, not component lifecycle
