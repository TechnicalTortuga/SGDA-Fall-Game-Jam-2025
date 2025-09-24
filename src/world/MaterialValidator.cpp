#include "MaterialValidator.h"
#include "../utils/Logger.h"
#include <filesystem>
#include <algorithm>

// Main validation entry point
MaterialValidator::ValidationResult MaterialValidator::ValidateMaterials(const MapData& mapData) {
    ValidationResult result;
    
    LOG_INFO("MaterialValidator: Starting validation for " + std::to_string(mapData.materials.size()) + 
             " materials and " + std::to_string(mapData.faces.size()) + " faces");

    // Validate material IDs are properly assigned
    if (!ValidateMaterialIds(mapData, result)) {
        LOG_ERROR("MaterialValidator: Material ID validation failed");
    }

    // Validate texture files exist
    if (!ValidateTextureFiles(mapData, result)) {
        LOG_WARNING("MaterialValidator: Some texture files are missing");
    }

    // Validate face UV coordinates
    if (!ValidateFaceUVs(mapData, result)) {
        LOG_WARNING("MaterialValidator: Some faces have invalid UV coordinates");
    }

    // Validate material properties
    if (!ValidateMaterialProperties(mapData, result)) {
        LOG_WARNING("MaterialValidator: Some materials have invalid properties");
    }

    // Log validation summary
    if (result.isValid) {
        LOG_INFO("MaterialValidator: Validation completed successfully");
    } else {
        LOG_ERROR("MaterialValidator: Validation failed with " + std::to_string(result.errors.size()) + 
                  " errors and " + std::to_string(result.warnings.size()) + " warnings");
    }

    return result;
}

// Validate that all material IDs referenced by faces exist in the materials list
bool MaterialValidator::ValidateMaterialIds(const MapData& mapData, ValidationResult& result) {
    std::unordered_set<int> usedMaterialIds = CollectUsedMaterialIds(mapData);
    std::unordered_set<int> availableMaterialIds;

    // Collect available material IDs
    for (const auto& material : mapData.materials) {
        availableMaterialIds.insert(material.id);
    }

    bool isValid = true;
    
    // Check if all used material IDs are available
    for (int materialId : usedMaterialIds) {
        if (availableMaterialIds.find(materialId) == availableMaterialIds.end()) {
            result.invalidMaterialIds.push_back(materialId);
            result.AddError("Material ID " + std::to_string(materialId) + " is used by faces but not defined in materials list");
            isValid = false;
        }
    }

    // Check for unused materials (warning only)
    for (const auto& material : mapData.materials) {
        if (usedMaterialIds.find(material.id) == usedMaterialIds.end()) {
            result.AddWarning("Material ID " + std::to_string(material.id) + " (" + material.name + ") is defined but not used by any faces");
        }
    }

    LOG_DEBUG("MaterialValidator: Found " + std::to_string(usedMaterialIds.size()) + " used materials, " +
              std::to_string(availableMaterialIds.size()) + " available materials");

    return isValid;
}

// Validate that texture files exist on disk
bool MaterialValidator::ValidateTextureFiles(const MapData& mapData, ValidationResult& result) {
    bool allTexturesExist = true;

    for (const auto& material : mapData.materials) {
        if (!material.diffuseMap.empty()) {
            if (!TextureExists(material.diffuseMap)) {
                result.missingTextures.push_back(material.diffuseMap);
                result.AddError("Texture file not found: " + material.diffuseMap + " (Material: " + material.name + ")");
                allTexturesExist = false;
            } else {
                LOG_DEBUG("MaterialValidator: Texture exists: " + material.diffuseMap);
            }
        }

        // Check other texture maps if they exist
        std::vector<std::pair<std::string, std::string>> textureMaps = {
            {material.normalMap, "normal"},
            {material.specularMap, "specular"},
            {material.roughnessMap, "roughness"},
            {material.metallicMap, "metallic"},
            {material.aoMap, "AO"},
            {material.emissiveMap, "emissive"}
        };

        for (const auto& [texturePath, textureType] : textureMaps) {
            if (!texturePath.empty() && !TextureExists(texturePath)) {
                result.AddWarning("Optional " + textureType + " texture not found: " + texturePath + " (Material: " + material.name + ")");
            }
        }
    }

    return allTexturesExist;
}

// Validate face UV coordinates
bool MaterialValidator::ValidateFaceUVs(const MapData& mapData, ValidationResult& result) {
    bool allUVsValid = true;
    int facesWithInvalidUVs = 0;
    int facesWithMissingUVs = 0;

    for (size_t i = 0; i < mapData.faces.size(); ++i) {
        const auto& face = mapData.faces[i];
        
        // Check if UV count matches vertex count
        if (face.uvs.size() != face.vertices.size()) {
            if (face.uvs.empty()) {
                facesWithMissingUVs++;
            } else {
                facesWithInvalidUVs++;
                result.AddWarning("Face " + std::to_string(i) + " has " + std::to_string(face.uvs.size()) + 
                                 " UVs but " + std::to_string(face.vertices.size()) + " vertices");
            }
            allUVsValid = false;
            continue;
        }

        // Check UV coordinate ranges and validity
        for (size_t j = 0; j < face.uvs.size(); ++j) {
            const auto& uv = face.uvs[j];
            
            if (std::isnan(uv.x) || std::isnan(uv.y) || std::isinf(uv.x) || std::isinf(uv.y)) {
                result.AddError("Face " + std::to_string(i) + " vertex " + std::to_string(j) + " has invalid UV coordinates");
                allUVsValid = false;
            } else if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f) {
                result.AddWarning("Face " + std::to_string(i) + " vertex " + std::to_string(j) + 
                                 " has UV coordinates outside 0-1 range: (" + std::to_string(uv.x) + "," + std::to_string(uv.y) + ")");
            }
        }
    }

    if (facesWithMissingUVs > 0) {
        result.AddWarning(std::to_string(facesWithMissingUVs) + " faces have no UV coordinates");
    }

    if (facesWithInvalidUVs > 0) {
        result.AddWarning(std::to_string(facesWithInvalidUVs) + " faces have mismatched UV/vertex counts");
    }

    LOG_DEBUG("MaterialValidator: UV validation completed for " + std::to_string(mapData.faces.size()) + " faces");

    return allUVsValid;
}

// Validate material properties
bool MaterialValidator::ValidateMaterialProperties(const MapData& mapData, ValidationResult& result) {
    bool allPropertiesValid = true;

    for (const auto& material : mapData.materials) {
        // Validate material ID
        if (material.id < 0) {
            result.AddError("Material '" + material.name + "' has invalid negative ID: " + std::to_string(material.id));
            allPropertiesValid = false;
        }

        // Validate material name
        if (material.name.empty()) {
            result.AddWarning("Material with ID " + std::to_string(material.id) + " has empty name");
        }

        // Validate numeric properties
        if (material.shininess < 0.0f || material.shininess > 1000.0f) {
            result.AddWarning("Material '" + material.name + "' has unusual shininess value: " + std::to_string(material.shininess));
        }

        if (material.alpha < 0.0f || material.alpha > 1.0f) {
            result.AddWarning("Material '" + material.name + "' has invalid alpha value: " + std::to_string(material.alpha));
        }

        if (material.roughness < 0.0f || material.roughness > 1.0f) {
            result.AddWarning("Material '" + material.name + "' has invalid roughness value: " + std::to_string(material.roughness));
        }

        if (material.metallic < 0.0f || material.metallic > 1.0f) {
            result.AddWarning("Material '" + material.name + "' has invalid metallic value: " + std::to_string(material.metallic));
        }
    }

    return allPropertiesValid;
}

// Check if texture file exists
bool MaterialValidator::TextureExists(const std::string& texturePath) {
    if (texturePath.empty()) {
        return false;
    }

    std::string fullPath = GetAssetPath(texturePath);
    return std::filesystem::exists(fullPath);
}

// Check if material exists in the materials list
bool MaterialValidator::HasValidMaterial(const MapData& mapData, int materialId) {
    return std::any_of(mapData.materials.begin(), mapData.materials.end(),
                      [materialId](const MaterialInfo& material) {
                          return material.id == materialId;
                      });
}

// Repair invalid materials by assigning default fallbacks
void MaterialValidator::RepairInvalidMaterials(MapData& mapData, const ValidationResult& result) {
    LOG_INFO("MaterialValidator: Repairing " + std::to_string(result.invalidMaterialIds.size()) + " invalid material references");

    // Create default material if missing
    int defaultMaterialId = 0;
    if (!HasValidMaterial(mapData, defaultMaterialId)) {
        MaterialInfo defaultMaterial;
        defaultMaterial.id = defaultMaterialId;
        defaultMaterial.name = "Default Material";
        defaultMaterial.type = "basic";
        defaultMaterial.diffuseMap = ""; // Will use white texture fallback
        defaultMaterial.diffuseColor = WHITE;
        mapData.materials.push_back(defaultMaterial);
        LOG_INFO("MaterialValidator: Created default material with ID " + std::to_string(defaultMaterialId));
    }

    // Replace invalid material IDs with default
    for (auto& face : mapData.faces) {
        if (std::find(result.invalidMaterialIds.begin(), result.invalidMaterialIds.end(), face.materialId) != result.invalidMaterialIds.end()) {
            LOG_DEBUG("MaterialValidator: Fixing face with invalid material ID " + std::to_string(face.materialId) + " -> " + std::to_string(defaultMaterialId));
            face.materialId = defaultMaterialId;
        }
    }
}

// Assign fallback textures for missing texture files
void MaterialValidator::AssignFallbackTextures(MapData& mapData, const ValidationResult& result) {
    LOG_INFO("MaterialValidator: Assigning fallback textures for " + std::to_string(result.missingTextures.size()) + " missing textures");

    const std::string fallbackTexture = "textures/devtextures/fallback_white.png";

    for (auto& material : mapData.materials) {
        if (std::find(result.missingTextures.begin(), result.missingTextures.end(), material.diffuseMap) != result.missingTextures.end()) {
            LOG_DEBUG("MaterialValidator: Assigning fallback texture to material '" + material.name + "'");
            material.diffuseMap = fallbackTexture;
        }
    }
}

// Collect all material IDs used by faces
std::unordered_set<int> MaterialValidator::CollectUsedMaterialIds(const MapData& mapData) {
    std::unordered_set<int> usedIds;
    
    for (const auto& face : mapData.faces) {
        usedIds.insert(face.materialId);
    }

    for (const auto& brush : mapData.brushes) {
        for (const auto& face : brush.faces) {
            usedIds.insert(face.materialId);
        }
    }

    return usedIds;
}

// Get full asset path from relative path
std::string MaterialValidator::GetAssetPath(const std::string& relativePath) {
    // Try different possible asset directories
    std::vector<std::string> assetPaths = {
        "assets/" + relativePath,
        "build/bin/assets/" + relativePath,
        "../assets/" + relativePath,
        relativePath  // Already absolute or relative to current directory
    };

    for (const auto& path : assetPaths) {
        if (std::filesystem::exists(path)) {
            return path;
        }
    }

    // Return the first guess if none exist
    return assetPaths[0];
}