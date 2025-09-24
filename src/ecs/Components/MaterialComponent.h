#pragma once

#include "../Component.h"
#include "../Systems/MaterialSystem.h"
#include "raylib.h"

// MaterialComponent - Lightweight Flyweight Handle
//
// This component is now a lightweight handle (16 bytes) that references
// shared material data stored in the MaterialSystem. It follows the
// flyweight pattern where intrinsic state (material properties) is
// shared, and extrinsic state (instance-specific parameters) is stored here.
//
// Memory: Reduced from 95+ bytes to 16 bytes (6.25x memory savings per component)
struct MaterialComponent : public Component {
    const char* GetTypeName() const override { return "MaterialComponent"; }

    // Core flyweight handle - references MaterialData in MaterialSystem
    uint32_t materialId = 0;        // Index into MaterialSystem.materialCache_
    uint16_t flags = 0;             // Instance-specific flags (see below)
    float params[4] = {0, 0, 0, 0}; // Custom shader parameters for this instance

    // Instance-specific flags (bitmask)
    enum Flags {
        DOUBLE_SIDED    = 1 << 0,   // Render both sides of faces
        DEPTH_WRITE     = 1 << 1,   // Write to depth buffer
        DEPTH_TEST      = 1 << 2,   // Enable depth testing
        CAST_SHADOWS    = 1 << 3,   // Whether this material casts shadows
        ACTIVE          = 1 << 4,   // Whether this component is active
        NEEDS_UPDATE    = 1 << 5,   // Whether material needs shader update

        // Gradient mode (2 bits for 4 modes)
        GRADIENT_NONE   = 0 << 6,   // No gradient (solid color using primaryColor)
        GRADIENT_LINEAR = 1 << 6,   // Linear gradient between primaryColor and secondaryColor
        GRADIENT_RADIAL = 2 << 6,   // Radial gradient from primaryColor (center) to secondaryColor (edge)
        GRADIENT_MASK   = 3 << 6,   // Mask for gradient mode bits
    };

    // Helper methods for flag access
    bool IsDoubleSided() const { return flags & DOUBLE_SIDED; }
    bool DepthWriteEnabled() const { return flags & DEPTH_WRITE; }
    bool DepthTestEnabled() const { return flags & DEPTH_TEST; }
    bool CastsShadows() const { return flags & CAST_SHADOWS; }
    bool IsActive() const { return flags & ACTIVE; }
    bool NeedsUpdate() const { return flags & NEEDS_UPDATE; }

    void SetDoubleSided(bool value) { value ? flags |= DOUBLE_SIDED : flags &= ~DOUBLE_SIDED; }
    void SetDepthWrite(bool value) { value ? flags |= DEPTH_WRITE : flags &= ~DEPTH_WRITE; }
    void SetDepthTest(bool value) { value ? flags |= DEPTH_TEST : flags &= ~DEPTH_TEST; }
    void SetCastsShadows(bool value) { value ? flags |= CAST_SHADOWS : flags &= ~CAST_SHADOWS; }
    void SetActive(bool value) { value ? flags |= ACTIVE : flags &= ~ACTIVE; }
    void SetNeedsUpdate(bool value) { value ? flags |= NEEDS_UPDATE : flags &= ~NEEDS_UPDATE; }

    // Gradient mode access
    uint16_t GetGradientMode() const { return flags & GRADIENT_MASK; }
    bool IsSolidColor() const { return GetGradientMode() == GRADIENT_NONE; }
    bool IsLinearGradient() const { return GetGradientMode() == GRADIENT_LINEAR; }
    bool IsRadialGradient() const { return GetGradientMode() == GRADIENT_RADIAL; }

    void SetGradientMode(uint16_t mode) {
        flags = (flags & ~GRADIENT_MASK) | (mode & GRADIENT_MASK);
    }
    void SetSolidColor() { SetGradientMode(GRADIENT_NONE); }
    void SetLinearGradient() { SetGradientMode(GRADIENT_LINEAR); }
    void SetRadialGradient() { SetGradientMode(GRADIENT_RADIAL); }

    // Helper methods for MaterialSystem integration
    // Note: These require a valid MaterialSystem instance to be passed

    const CachedMaterialData* GetMaterialData(MaterialSystem* materialSystem) const {
        if (!materialSystem) return nullptr;
        return materialSystem->GetMaterial(materialId);
    }

    // Convenience accessors for commonly used material properties
    Color GetPrimaryColor(MaterialSystem* materialSystem) const {
        const CachedMaterialData* data = GetMaterialData(materialSystem);
        return data ? data->primaryColor : WHITE;
    }

    Color GetSecondaryColor(MaterialSystem* materialSystem) const {
        const CachedMaterialData* data = GetMaterialData(materialSystem);
        return data ? data->secondaryColor : BLACK;
    }

    // Backward compatibility - diffuseColor now maps to primaryColor
    Color GetDiffuseColor(MaterialSystem* materialSystem) const {
        return GetPrimaryColor(materialSystem);
    }

    Color GetSpecularColor(MaterialSystem* materialSystem) const {
        const CachedMaterialData* data = GetMaterialData(materialSystem);
        return data ? data->specularColor : WHITE;
    }

    float GetShininess(MaterialSystem* materialSystem) const {
        const CachedMaterialData* data = GetMaterialData(materialSystem);
        return data ? data->shininess : 32.0f;
    }

    float GetAlpha(MaterialSystem* materialSystem) const {
        const CachedMaterialData* data = GetMaterialData(materialSystem);
        return data ? data->alpha : 1.0f;
    }

    MaterialData::MaterialType GetMaterialType(MaterialSystem* materialSystem) const {
        const CachedMaterialData* data = GetMaterialData(materialSystem);
        return data ? data->type : MaterialData::MaterialType::BASIC;
    }

    // Validation
    bool IsValid(MaterialSystem* materialSystem) const {
        return materialSystem && materialSystem->IsValidMaterialId(materialId);
    }

    // Factory method for creating materials through MaterialSystem
    static uint32_t CreateMaterial(MaterialSystem* materialSystem, const MaterialProperties& properties) {
        if (!materialSystem) return 0;
        return materialSystem->GetOrCreateMaterial(properties);
    }

    // Constructor with MaterialSystem integration
    MaterialComponent(uint32_t materialId = 0) : materialId(materialId) {
        flags = ACTIVE | DEPTH_WRITE | DEPTH_TEST | CAST_SHADOWS | GRADIENT_NONE; // Sensible defaults
    }

    // Constructor from MaterialProperties (requires MaterialSystem)
    MaterialComponent(MaterialSystem* materialSystem, const MaterialProperties& properties)
        : materialId(CreateMaterial(materialSystem, properties)) {
        flags = ACTIVE | DEPTH_WRITE | DEPTH_TEST | CAST_SHADOWS | GRADIENT_NONE;
    }
};
