#pragma once

#include "../Component.h"
#include "../Systems/AssetSystem.h"
#include "raylib.h"
#include <string>

/*
MaterialComponent - Pure data material component for ECS

This struct contains ONLY essential material data. All material operations
(matrix calculations, shader updates, preset management) are handled by
dedicated systems that reference this component by entity ID.

This is the purest form of data-oriented ECS design for materials.
*/

struct MaterialComponent : public Component {
    // Component type identification
    const char* GetTypeName() const override { return "MaterialComponent"; }

    // Material types for different rendering approaches
    enum class MaterialType {
        BASIC,      // Simple diffuse + specular
        PBR,        // Physically-based rendering
        EMISSIVE,   // Glowing materials
        TRANSPARENT // Transparent materials
    };

    // Core material properties (pure data only - NO methods)
    Color diffuseColor = {255, 255, 255, 255};
    Color specularColor = {255, 255, 255, 255};
    float shininess = 32.0f;

    // PBR properties (pure data)
    float roughness = 0.5f;
    float metallic = 0.0f;

    // Material type (pure data)
    MaterialType type = MaterialType::BASIC;

    // Texture resource handles (data-oriented - direct resource access)
    AssetSystem::TextureHandle diffuseTextureHandle;
    AssetSystem::TextureHandle normalTextureHandle;
    AssetSystem::TextureHandle specularTextureHandle;
    AssetSystem::TextureHandle roughnessTextureHandle;
    AssetSystem::TextureHandle metallicTextureHandle;

    // Reference IDs for related systems (data-oriented decoupling)
    uint64_t materialSystemId = 0;     // For material management
    uint64_t shaderSystemId = 0;       // For shader integration
    uint64_t textureSystemId = 0;      // For texture coordination

    // Material metadata (pure data)
    std::string materialName = "default";

    // Simple state flags (pure data)
    bool isActive = true;
    bool needsShaderUpdate = true;
};
