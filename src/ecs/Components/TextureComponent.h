#pragma once

#include "../Component.h"
#include "../Systems/AssetSystem.h"
#include <string>
#include "raylib.h"

/*
TextureComponent - Pure data texture component for ECS

This struct contains ONLY essential texture metadata. All texture operations
(loading, binding, memory management) are handled by the AssetSystem which
provides lightweight resource handles.

This is the purest form of data-oriented ECS design for textures.
*/

struct TextureComponent : public Component {
    // Component type identification
    const char* GetTypeName() const override { return "TextureComponent"; }

    // Texture types for different use cases
    enum class TextureType {
        DIFFUSE,    // Main color texture
        NORMAL,     // Normal map for surface detail
        SPECULAR,   // Specular/gloss map
        ROUGHNESS,  // Roughness map for PBR
        METALLIC,   // Metallic map for PBR
        EMISSIVE,   // Emissive/glow texture
        AMBIENT_OCCLUSION,  // Ambient occlusion map
        HEIGHT      // Height/displacement map
    };

    // Texture metadata (pure data only - NO methods)
    TextureType type = TextureType::DIFFUSE;
    std::string texturePath = "";      // Original file path for loading
    Texture2D texture = {0, 0, 0, 0, 0}; // Actual texture data
    AssetSystem::TextureHandle textureHandle; // Resource handle for safe access

    // Reference IDs for related systems (data-oriented decoupling)
    uint64_t assetSystemId = 0;        // For resource coordination
    uint64_t materialSystemId = 0;     // For material integration

    // Texture properties (pure data)
    int width = 0;
    int height = 0;
    int mipmaps = 1;
    int format = 0;  // Raylib pixel format

    // Simple state flags (pure data)
    bool isActive = true;
    bool isLoaded = false;
    bool needsReload = false;

    // AssetSystem integration
    // Note: Use textureHandle to access actual texture resource
    // This component only stores metadata and lightweight references
};
