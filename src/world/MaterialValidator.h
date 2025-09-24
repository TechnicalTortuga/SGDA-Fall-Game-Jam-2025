#pragma once

#include "MapLoader.h"
#include <vector>
#include <string>
#include <unordered_set>

// Material validation system for ensuring data integrity throughout the rendering pipeline
class MaterialValidator {
public:
    struct ValidationResult {
        bool isValid = true;
        std::vector<std::string> missingTextures;
        std::vector<int> invalidMaterialIds;
        std::vector<std::string> warnings;
        std::vector<std::string> errors;
        
        void AddWarning(const std::string& warning) {
            warnings.push_back(warning);
        }
        
        void AddError(const std::string& error) {
            errors.push_back(error);
            isValid = false;
        }
    };

    MaterialValidator() = default;
    ~MaterialValidator() = default;

    // Main validation entry point
    ValidationResult ValidateMaterials(const MapData& mapData);

    // Individual validation functions
    bool ValidateMaterialIds(const MapData& mapData, ValidationResult& result);
    bool ValidateTextureFiles(const MapData& mapData, ValidationResult& result);
    bool ValidateFaceUVs(const MapData& mapData, ValidationResult& result);
    bool ValidateMaterialProperties(const MapData& mapData, ValidationResult& result);

    // Utility functions
    bool TextureExists(const std::string& texturePath);
    bool HasValidMaterial(const MapData& mapData, int materialId);
    
    // Repair functions
    void RepairInvalidMaterials(MapData& mapData, const ValidationResult& result);
    void AssignFallbackTextures(MapData& mapData, const ValidationResult& result);

private:
    // Internal validation helpers
    std::unordered_set<int> CollectUsedMaterialIds(const MapData& mapData);
    std::string GetAssetPath(const std::string& relativePath);
};