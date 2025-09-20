#pragma once

#include "../Component.h"
#include "raylib.h"
#include <string>
#include <memory>

class Sprite : public Component {
public:
    Sprite();
    Sprite(const std::string& texturePath);
    Sprite(const std::string& texturePath, float scale, float rotation);
    ~Sprite();

    // Texture management
    bool LoadTexture(const std::string& texturePath);
    void UnloadTexture();
    bool IsTextureLoaded() const { return textureLoaded_; }

    // Getters
    Texture2D GetTexture() const { return texture_; }
    const std::string& GetTexturePath() const { return texturePath_; }
    float GetScale() const { return scale_; }
    float GetRotation() const { return rotation_; }
    Color GetTint() const { return tint_; }
    float GetLightIntensity() const { return lightIntensity_; }

    // Setters
    void SetScale(float scale) { scale_ = scale; }
    void SetRotation(float rotation) { rotation_ = rotation; }
    void SetTint(Color tint) { tint_ = tint; }
    void SetLightIntensity(float intensity) { lightIntensity_ = intensity; }

    // Decal overlay for paint effects
    Texture2D GetDecalOverlay() const { return decalOverlay_; }
    void SetDecalOverlay(Texture2D overlay) { decalOverlay_ = overlay; }
    void ClearDecalOverlay();

    // Rendering properties
    Rectangle GetSourceRect() const;
    Vector2 GetOrigin() const;
    float GetWidth() const;
    float GetHeight() const;

    // Component type identification
    const char* GetTypeName() const override { return "Sprite"; }

private:
    Texture2D texture_;
    std::string texturePath_;
    bool textureLoaded_;

    float scale_;
    float rotation_;
    Color tint_;
    float lightIntensity_;

    Texture2D decalOverlay_;
};
