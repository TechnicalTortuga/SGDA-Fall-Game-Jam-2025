#include "MapLoader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include "../utils/Logger.h"
#include "../utils/StringUtils.h"

MapLoader::MapLoader() {}

MapData MapLoader::LoadMap(const std::string& mapPath) {
    LOG_INFO("Parsing map file: " + mapPath);

    MapData mapData;

    // Try to load the map file
    std::ifstream file(mapPath);
    if (!file.is_open()) {
        LOG_WARNING("Map file not found: " + mapPath);
        return MapData{}; // Return empty MapData
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    if (!ParseMapFile(content, mapData)) {
        LOG_ERROR("Failed to parse map file: " + mapPath);
        return MapData{}; // Return empty MapData
    }

    LOG_INFO("Map parsing completed successfully. Faces: " +
              std::to_string(mapData.faces.size()) +
              ", Materials: " + std::to_string(mapData.materials.size()));
    return mapData;
}


bool MapLoader::ParseMapFile(const std::string& content, MapData& mapData) {
    // Only support YAML format for new maps
    if (content.find("version:") != std::string::npos ||
        content.find("# PaintSplash Map Format") != std::string::npos) {
        LOG_INFO("Parsing YAML map format");
        return ParseYamlMap(content, mapData);
    }

    LOG_ERROR("Unsupported map format - only YAML format is supported");
    return false;
}



// YAML map format parsing implementation
bool MapLoader::ParseYamlMap(const std::string& content, MapData& mapData) {
    try {
        LOG_INFO("ParseYamlMap: Content length: " + std::to_string(content.length()));
        if (content.length() < 500) {
            LOG_INFO("ParseYamlMap: Full content:\n" + content);
        }

        // Extract basic map information
        mapData.name = ExtractYamlValue(content, "name");
        if (mapData.name.empty()) {
            mapData.name = "Untitled Map";
        }

        // Parse materials section
        std::string materialsBlock = ExtractYamlBlock(content, "materials");
        LOG_DEBUG("Materials block extraction - length: " + std::to_string(materialsBlock.length()));
        if (!materialsBlock.empty()) {
            ParseMaterials(materialsBlock, mapData);
        }

        // Parse entities section
        std::string entitiesBlock = ExtractYamlBlock(content, "entities");
        LOG_DEBUG("Entities block extraction - length: " + std::to_string(entitiesBlock.length()));
        if (!entitiesBlock.empty()) {
            ParseEntities(entitiesBlock, mapData);
        }

        // Parse world geometry (brushes)
        LOG_INFO("About to extract world block from content of length: " + std::to_string(content.length()));
        std::string worldBlock = ExtractYamlBlock(content, "world");
        LOG_INFO("World block extraction - length: " + std::to_string(worldBlock.length()));
        if (worldBlock.length() < 200) {
            LOG_INFO("World block content:\n" + worldBlock);
        }
        if (!worldBlock.empty()) {
            ParseWorldGeometry(worldBlock, mapData);
        } else {
            LOG_WARNING("No world block found in YAML");
        }

        LOG_INFO("YAML map parsed successfully. Faces: " + std::to_string(mapData.faces.size()) +
                 ", Entities: " + std::to_string(mapData.entities.size()));
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse YAML map: " + std::string(e.what()));
        return false;
    }
}

bool MapLoader::ParseMaterials(const std::string& materialsYaml, MapData& mapData) {
    LOG_INFO("Parsing materials section");
    LOG_INFO("Materials YAML content length: " + std::to_string(materialsYaml.length()));
    if (materialsYaml.length() < 200) {
        LOG_INFO("Materials YAML content:\n" + materialsYaml);
    }

    // Parse individual materials
    std::vector<std::string> materialItems = ExtractYamlList(materialsYaml, "");
    LOG_INFO("Found " + std::to_string(materialItems.size()) + " material items");

    for (const auto& materialItem : materialItems) {
        MaterialInfo material;

        // Extract ID
        std::string idStr = ExtractYamlValue(materialItem, "id");
        if (!idStr.empty()) {
            material.id = std::stoi(idStr);
        }

        // Extract name (for now, also check if it's actually a texture path for legacy compatibility)
        std::string nameValue = ExtractYamlValue(materialItem, "name");

        // For legacy compatibility: if name looks like a texture path and no diffuseMap is specified,
        // treat name as the diffuse texture path and generate a proper material name
        if (nameValue.find("textures/") != std::string::npos || nameValue.find(".png") != std::string::npos) {
            material.diffuseMap = nameValue;
            LOG_DEBUG("Set diffuseMap from name: '" + material.diffuseMap + "'");
            // Generate a proper material name from the texture path
            size_t lastSlash = nameValue.find_last_of('/');
            size_t lastDot = nameValue.find_last_of('.');
            if (lastSlash != std::string::npos && lastDot != std::string::npos && lastDot > lastSlash) {
                material.name = nameValue.substr(lastSlash + 1, lastDot - lastSlash - 1);
                // Capitalize first letter
                if (!material.name.empty()) {
                    material.name[0] = toupper(material.name[0]);
                }
                material.name += " Material";
            } else {
                // Fallback to a generic name
                material.name = "Legacy Material";
            }
        } else {
            // Use the name as-is for new format materials
            material.name = nameValue;
        }

        // Extract material type
        std::string typeStr = ExtractYamlValue(materialItem, "type");
        if (!typeStr.empty()) {
            material.type = typeStr;
        }

        // BASIC MATERIAL PROPERTIES
        // ------------------------
        std::string diffuseColorStr = ExtractYamlValue(materialItem, "diffuseColor");
        if (!diffuseColorStr.empty()) {
            material.diffuseColor = ParseColor(diffuseColorStr);
        }

        std::string specularColorStr = ExtractYamlValue(materialItem, "specularColor");
        if (!specularColorStr.empty()) {
            material.specularColor = ParseColor(specularColorStr);
        }

        std::string shininessStr = ExtractYamlValue(materialItem, "shininess");
        if (!shininessStr.empty()) {
            material.shininess = std::stof(shininessStr);
        }

        std::string alphaStr = ExtractYamlValue(materialItem, "alpha");
        if (!alphaStr.empty()) {
            material.alpha = std::stof(alphaStr);
        }

        // PBR PROPERTIES
        // --------------
        std::string roughnessStr = ExtractYamlValue(materialItem, "roughness");
        if (!roughnessStr.empty()) {
            material.roughness = std::stof(roughnessStr);
        }

        std::string metallicStr = ExtractYamlValue(materialItem, "metallic");
        if (!metallicStr.empty()) {
            material.metallic = std::stof(metallicStr);
        }

        std::string aoStr = ExtractYamlValue(materialItem, "ao");
        if (!aoStr.empty()) {
            material.ao = std::stof(aoStr);
        }

        // EMISSION PROPERTIES
        // ------------------
        std::string emissiveColorStr = ExtractYamlValue(materialItem, "emissiveColor");
        if (!emissiveColorStr.empty()) {
            material.emissiveColor = ParseColor(emissiveColorStr);
        }

        std::string emissiveIntensityStr = ExtractYamlValue(materialItem, "emissiveIntensity");
        if (!emissiveIntensityStr.empty()) {
            material.emissiveIntensity = std::stof(emissiveIntensityStr);
        }

        // TEXTURE MAPS
        // ------------
        std::string yamlDiffuseMap = ExtractYamlValue(materialItem, "diffuseMap");
        if (!yamlDiffuseMap.empty()) {
            material.diffuseMap = yamlDiffuseMap;
        }
        material.normalMap = ExtractYamlValue(materialItem, "normalMap");
        material.specularMap = ExtractYamlValue(materialItem, "specularMap");
        material.roughnessMap = ExtractYamlValue(materialItem, "roughnessMap");
        material.metallicMap = ExtractYamlValue(materialItem, "metallicMap");
        material.aoMap = ExtractYamlValue(materialItem, "aoMap");
        material.emissiveMap = ExtractYamlValue(materialItem, "emissiveMap");

        // RENDERING FLAGS
        // ---------------
        std::string doubleSidedStr = ExtractYamlValue(materialItem, "doubleSided");
        if (!doubleSidedStr.empty()) {
            material.doubleSided = (doubleSidedStr == "true");
        }

        std::string depthWriteStr = ExtractYamlValue(materialItem, "depthWrite");
        if (!depthWriteStr.empty()) {
            material.depthWrite = (depthWriteStr == "true");
        }

        std::string depthTestStr = ExtractYamlValue(materialItem, "depthTest");
        if (!depthTestStr.empty()) {
            material.depthTest = (depthTestStr == "true");
        }

        std::string castShadowsStr = ExtractYamlValue(materialItem, "castShadows");
        if (!castShadowsStr.empty()) {
            material.castShadows = (castShadowsStr == "true");
        }

        mapData.materials.push_back(material);
        LOG_INFO("Parsed material: ID=" + std::to_string(material.id) +
                 ", name='" + material.name + "', type='" + material.type + "'");
    }

    return true;
}

bool MapLoader::ParseWorldGeometry(const std::string& worldYaml, MapData& mapData) {
    LOG_INFO("Parsing world geometry from YAML");
    LOG_INFO("World YAML content length: " + std::to_string(worldYaml.length()));
    if (worldYaml.length() < 200) {
        LOG_INFO("World YAML content:\n" + worldYaml);
    } else {
        LOG_INFO("World YAML content (first 200 chars):\n" + worldYaml.substr(0, 200) + "...");
    }

    // Extract brushes section
    std::string brushesBlock = ExtractYamlBlock(worldYaml, "brushes");
    LOG_INFO("Extracted brushes block length: " + std::to_string(brushesBlock.length()));
    if (brushesBlock.length() < 200) {
        LOG_INFO("Extracted brushes block:\n" + brushesBlock);
    } else {
        LOG_INFO("Extracted brushes block (first 200 chars):\n" + brushesBlock.substr(0, 200) + "...");
    }
    if (brushesBlock.empty()) {
        LOG_WARNING("No brushes found in world geometry");
        return true;
    }

    // Parse individual brushes
    std::vector<std::string> brushItems = ExtractYamlList(brushesBlock, "");
    LOG_INFO("Found " + std::to_string(brushItems.size()) + " brushes to parse");
    if (brushItems.size() < 5) {
        LOG_INFO("First few brush items:");
        for (size_t i = 0; i < std::min(brushItems.size(), size_t(3)); ++i) {
            LOG_INFO("Brush " + std::to_string(i) + " (first 100 chars): " + brushItems[i].substr(0, 100));
        }
    }

    for (const auto& brushItem : brushItems) {
        // Parse each brush
        if (!ParseBrush(brushItem, mapData)) {
            LOG_ERROR("Failed to parse brush");
            return false;
        }
    }

    LOG_INFO("World geometry parsing completed - " + std::to_string(mapData.faces.size()) + " faces loaded");
    if (mapData.faces.empty()) {
        LOG_ERROR("No faces were parsed from world geometry!");
    } else {
        LOG_INFO("First face has " + std::to_string(mapData.faces[0].vertices.size()) + " vertices, material " + std::to_string(mapData.faces[0].materialId));
    }
    return true;
}

bool MapLoader::ParseBrush(const std::string& brushYaml, MapData& mapData) {
    LOG_DEBUG("ParseBrush: brushYaml length: " + std::to_string(brushYaml.length()));
    if (brushYaml.length() < 200) {
        LOG_DEBUG("ParseBrush: brushYaml content:\n" + brushYaml);
    }

    // Extract faces section
    std::string facesBlock = ExtractYamlBlock(brushYaml, "faces");
    LOG_DEBUG("ParseBrush: facesBlock length: " + std::to_string(facesBlock.length()));
    if (facesBlock.empty()) {
        LOG_WARNING("No faces found in brush");
        return true; // Empty brush is ok
    }

    // Parse individual faces
    std::vector<std::string> faceItems = ExtractYamlList(facesBlock, "");
    LOG_DEBUG("Found " + std::to_string(faceItems.size()) + " faces in brush");

    for (const auto& faceItem : faceItems) {
        // Parse each face
        if (!ParseBrushFace(faceItem, mapData)) {
            LOG_ERROR("Failed to parse face");
            return false;
        }
    }

    return true;
}

bool MapLoader::ParseBrushFace(const std::string& faceYaml, MapData& mapData) {
    Face face;

    // Extract material
    std::string materialStr = ExtractYamlValue(faceYaml, "material");
    if (!materialStr.empty()) {
        try {
            face.materialId = std::stoi(materialStr);
        } catch (const std::exception&) {
            LOG_WARNING("Invalid material ID in face: " + materialStr);
            face.materialId = 0;
        }
    }

    // Extract tint
    std::string tintStr = ExtractYamlValue(faceYaml, "tint");
    if (!tintStr.empty()) {
        face.tint = ParseColor(tintStr);
    } else {
        face.tint = WHITE;
    }

    // Extract render mode
    std::string renderModeStr = ExtractYamlValue(faceYaml, "render_mode");
    if (!renderModeStr.empty()) {
        if (renderModeStr == "default") {
            face.renderMode = FaceRenderMode::Default;
        } else if (renderModeStr == "vertex_colors") {
            face.renderMode = FaceRenderMode::VertexColors;
        } else if (renderModeStr == "wireframe") {
            face.renderMode = FaceRenderMode::Wireframe;
        } else if (renderModeStr == "invisible") {
            face.renderMode = FaceRenderMode::Invisible;
        } else {
            LOG_WARNING("Unknown render mode in face: " + renderModeStr);
            face.renderMode = FaceRenderMode::Default;
        }
    }

    // Extract vertices
    std::string verticesBlock = ExtractYamlBlock(faceYaml, "vertices");
    if (verticesBlock.empty()) {
        LOG_ERROR("No vertices found in face");
        return false;
    }

    std::vector<std::string> vertexItems = ExtractYamlList(verticesBlock, "");
    for (const auto& vertexItem : vertexItems) {
        Vector3 vertex = ParseVector3(vertexItem);
        face.vertices.push_back(vertex);
    }

    if (face.vertices.size() < 3) {
        LOG_ERROR("Face must have at least 3 vertices, found " + std::to_string(face.vertices.size()));
        return false;
    }

    // Extract UV coordinates (if provided)
    std::string uvBlock = ExtractYamlBlock(faceYaml, "uvs");
    if (!uvBlock.empty()) {
        std::vector<std::string> uvItems = ExtractYamlList(uvBlock, "");
        for (const auto& uvItem : uvItems) {
            Vector2 uv = ParseVector2(uvItem);
            // Transform UV coordinates from [-0.5, 0.5] range to [0, 1] range for OpenGL
            uv.x += 0.5f;
            uv.y += 0.5f;
            // Flip V coordinate so (0,0) is top-left instead of bottom-left
            uv.y = 1.0f - uv.y;
            face.uvs.push_back(uv);
        }

        // Ensure we have matching UVs for vertices
        if (face.uvs.size() != face.vertices.size()) {
            LOG_WARNING("UV count (" + std::to_string(face.uvs.size()) + ") doesn't match vertex count (" +
                       std::to_string(face.vertices.size()) + "), using default UVs");
            face.uvs.clear();
        }
    }

    // UVs should be provided in YAML - if not, leave empty (no default generation)
    if (face.uvs.empty()) {
        LOG_DEBUG("ParseBrushFace: No UVs provided in YAML");
    } else {
        LOG_DEBUG("ParseBrushFace: UVs were provided (" + std::to_string(face.uvs.size()) + " coordinates)");
    }

    LOG_DEBUG("ParseBrushFace: Final face has " + std::to_string(face.vertices.size()) + " vertices and " +
              std::to_string(face.uvs.size()) + " UVs, material=" + std::to_string(face.materialId));

    // Calculate normal
    face.RecalculateNormal();

    // Add face to map data
    mapData.faces.push_back(face);

    return true;
}

bool MapLoader::ParseEntities(const std::string& yamlContent, MapData& mapData) {
    std::stringstream ss(yamlContent);
    std::string line;
    std::vector<std::string> entityBlocks;
    std::string currentEntityBlock;
    int baseIndent = -1;
    bool inEntity = false;

    // Split YAML into individual entity blocks
    while (std::getline(ss, line)) {
        int indent = GetYamlIndentation(line);
        std::string trimmed = StringUtils::Trim(line);

        if (trimmed.empty() || trimmed[0] == '#') continue;

        // Check for entity start (looking for "- id:" pattern)
        if (trimmed.find("- id:") == 0) {
            if (inEntity && !currentEntityBlock.empty()) {
                entityBlocks.push_back(currentEntityBlock);
                currentEntityBlock.clear();
            }
            inEntity = true;
            baseIndent = indent;
            currentEntityBlock = line + "\n";
        } else if (inEntity) {
            if (indent >= baseIndent) {
                currentEntityBlock += line + "\n";
            } else {
                // End of current entity
                entityBlocks.push_back(currentEntityBlock);
                currentEntityBlock.clear();
                inEntity = false;
            }
        }
    }

    // Add the last entity if it exists
    if (inEntity && !currentEntityBlock.empty()) {
        entityBlocks.push_back(currentEntityBlock);
    }

    // Parse each entity block
    for (size_t i = 0; i < entityBlocks.size(); ++i) {
        try {
            auto entity = std::make_unique<EntityDefinition>(ParseEntity(entityBlocks[i], static_cast<uint32_t>(i + 1000)));
            mapData.entities.push_back(std::move(entity));
        } catch (const std::exception& e) {
            LOG_WARNING("Failed to parse entity " + std::to_string(i) + ": " + std::string(e.what()));
        }
    }

    return true;
}

EntityDefinition MapLoader::ParseEntity(const std::string& entityYaml, uint32_t id) {
    EntityDefinition entity;
    entity.id = id;

    // Extract basic entity properties
    entity.name = ExtractYamlValue(entityYaml, "name");
    std::string className = ExtractYamlValue(entityYaml, "class");

    // Determine entity type based on class name
    if (className == "light_point") {
        entity.type = GameObjectType::LIGHT_POINT;
        entity.className = className;
    } else if (className == "light_spot") {
        entity.type = GameObjectType::LIGHT_SPOT;
        entity.className = className;
    } else if (className == "light_directional") {
        entity.type = GameObjectType::LIGHT_DIRECTIONAL;
        entity.className = className;
    } else if (className == "audio_source") {
        entity.type = GameObjectType::AUDIO_SOURCE;
        entity.className = className;
    } else if (className == "player_start") {
        entity.type = GameObjectType::SPAWN_POINT;
        entity.className = className;
        entity.spawnPoint.type = SpawnPointType::PLAYER;
    } else if (className == "ai_waypoint") {
        entity.type = GameObjectType::WAYPOINT;
        entity.className = className;
            } else {
        entity.type = GameObjectType::STATIC_PROP;
        entity.className = className;
    }

    // Parse transform
    std::string transformBlock = ExtractYamlBlock(entityYaml, "transform");
    if (!transformBlock.empty()) {
        // Parse position
        std::string posStr = ExtractYamlValue(transformBlock, "position");
        if (!posStr.empty()) {
            entity.position = ParseVector3(posStr);
        }

        // Parse rotation - multiple formats supported
        entity.rotation = {0, 0, 0, 1}; // Default identity quaternion

        // Try quaternion format first: rotation: [x, y, z, w]
        std::string rotStr = ExtractYamlValue(transformBlock, "rotation");
        if (!rotStr.empty()) {
            entity.rotation = ParseQuaternion(rotStr);
        }
        // Try Euler angles: rotation_euler: [pitch, yaw, roll] (degrees)
        else {
            std::string eulerStr = ExtractYamlValue(transformBlock, "rotation_euler");
            if (!eulerStr.empty()) {
                Vector3 euler = ParseVector3(eulerStr);
                entity.rotation = QuaternionFromEuler(euler.x * DEG2RAD, euler.y * DEG2RAD, euler.z * DEG2RAD);
            }
            // Try axis-angle: rotation_axis_angle: [[x, y, z], angle]
            else {
                std::string axisAngleStr = ExtractYamlValue(transformBlock, "rotation_axis_angle");
                if (!axisAngleStr.empty()) {
                    // This is a complex format: [[x,y,z], angle]
                    // For now, skip complex parsing and use default
                    LOG_WARNING("rotation_axis_angle format not yet supported, using identity rotation");
                }
            }
        }

        // Parse scale
        std::string scaleStr = ExtractYamlValue(transformBlock, "scale");
        if (!scaleStr.empty()) {
            entity.scale = ParseVector3(scaleStr);
        }

        // Parse parent entity ID (for hierarchy)
        std::string parentStr = ExtractYamlValue(transformBlock, "parent");
        if (!parentStr.empty()) {
            // Store parent ID in properties for later processing
            entity.properties["parent_id"] = parentStr;
        }
    }

    // Parse properties based on entity type
    std::string propertiesBlock = ExtractYamlBlock(entityYaml, "properties");
    if (!propertiesBlock.empty()) {
        if (entity.type == GameObjectType::LIGHT_POINT ||
            entity.type == GameObjectType::LIGHT_SPOT ||
            entity.type == GameObjectType::LIGHT_DIRECTIONAL) {

            // Parse light type
            std::string typeStr = ExtractYamlValue(propertiesBlock, "type");
            if (!typeStr.empty()) {
                if (typeStr == "point") {
                    entity.light.type = LightType::POINT;
                } else if (typeStr == "spot") {
                    entity.light.type = LightType::SPOT;
                } else if (typeStr == "directional") {
                    entity.light.type = LightType::DIRECTIONAL;
                }
            }

            // Parse color
            std::string colorStr = ExtractYamlValue(propertiesBlock, "color");
            if (!colorStr.empty()) {
                entity.light.color = ParseColor(colorStr);
            }

            // Parse intensity
            std::string intensityStr = ExtractYamlValue(propertiesBlock, "intensity");
            if (!intensityStr.empty()) {
                entity.light.intensity = std::stof(intensityStr);
            }

            // Parse common shadow properties
            std::string castShadowsStr = ExtractYamlValue(propertiesBlock, "castShadows");
            if (!castShadowsStr.empty()) {
                entity.light.castShadows = (castShadowsStr == "true");
            }

            std::string shadowBiasStr = ExtractYamlValue(propertiesBlock, "shadowBias");
            if (!shadowBiasStr.empty()) {
                entity.light.shadowBias = std::stof(shadowBiasStr);
            }

            // Point light specific properties
            if (entity.type == GameObjectType::LIGHT_POINT) {
                std::string radiusStr = ExtractYamlValue(propertiesBlock, "range"); // Note: spec uses "range" for point lights
                if (!radiusStr.empty()) {
                    entity.light.radius = std::stof(radiusStr);
                }

                std::string shadowResolutionStr = ExtractYamlValue(propertiesBlock, "shadowMapSize");
                if (!shadowResolutionStr.empty()) {
                    entity.light.shadowResolution = std::stoi(shadowResolutionStr);
                }
            }
            // Spot light specific properties
            else if (entity.type == GameObjectType::LIGHT_SPOT) {
                std::string rangeStr = ExtractYamlValue(propertiesBlock, "range");
                if (!rangeStr.empty()) {
                    entity.light.range = std::stof(rangeStr);
                }

                std::string innerAngleStr = ExtractYamlValue(propertiesBlock, "innerAngle");
                if (!innerAngleStr.empty()) {
                    entity.light.innerAngle = std::stof(innerAngleStr);
                }

                std::string outerAngleStr = ExtractYamlValue(propertiesBlock, "outerAngle");
                if (!outerAngleStr.empty()) {
                    entity.light.outerAngle = std::stof(outerAngleStr);
                }

                std::string shadowResolutionStr = ExtractYamlValue(propertiesBlock, "shadowMapSize");
                if (!shadowResolutionStr.empty()) {
                    entity.light.shadowResolution = std::stoi(shadowResolutionStr);
                }
            }
            // Directional light specific properties
            else if (entity.type == GameObjectType::LIGHT_DIRECTIONAL) {
                std::string shadowMapSizeStr = ExtractYamlValue(propertiesBlock, "shadowMapSize");
                if (!shadowMapSizeStr.empty()) {
                    entity.light.shadowMapSize = std::stoi(shadowMapSizeStr);
                }

                std::string shadowDistanceStr = ExtractYamlValue(propertiesBlock, "shadowDistance");
                if (!shadowDistanceStr.empty()) {
                    entity.light.shadowDistance = std::stof(shadowDistanceStr);
                }

                std::string cascadeCountStr = ExtractYamlValue(propertiesBlock, "shadowCascadeCount");
                if (!cascadeCountStr.empty()) {
                    entity.light.shadowCascadeCount = std::stoi(cascadeCountStr);
                }
            }

            // Parse enabled state
            std::string enabledStr = ExtractYamlValue(propertiesBlock, "enabled");
            if (!enabledStr.empty()) {
                entity.light.enabled = (enabledStr == "true");
            }

        } else if (entity.type == GameObjectType::AUDIO_SOURCE) {
            // Parse audio type
            std::string audioTypeStr = ExtractYamlValue(propertiesBlock, "audioType");
            if (!audioTypeStr.empty()) {
                if (audioTypeStr == "SFX_3D") {
                    entity.audio.audioType = AudioComponent::AudioType::SFX_3D;
                } else if (audioTypeStr == "SFX_2D") {
                    entity.audio.audioType = AudioComponent::AudioType::SFX_2D;
                } else if (audioTypeStr == "MUSIC") {
                    entity.audio.audioType = AudioComponent::AudioType::MUSIC;
                } else if (audioTypeStr == "UI") {
                    entity.audio.audioType = AudioComponent::AudioType::UI;
                } else if (audioTypeStr == "AMBIENT") {
                    entity.audio.audioType = AudioComponent::AudioType::AMBIENT;
                } else if (audioTypeStr == "VOICE") {
                    entity.audio.audioType = AudioComponent::AudioType::VOICE;
                } else if (audioTypeStr == "MASTER") {
                    entity.audio.audioType = AudioComponent::AudioType::MASTER;
                } else {
                    entity.audio.audioType = AudioComponent::AudioType::SFX_3D; // Default
                }
            }

            // Parse audio clip path
            std::string clipStr = ExtractYamlValue(propertiesBlock, "clip");
            if (!clipStr.empty()) {
                entity.audio.clipPath = clipStr;
            }

            // Parse basic audio properties
            std::string volumeStr = ExtractYamlValue(propertiesBlock, "volume");
            if (!volumeStr.empty()) {
                entity.audio.volume = std::stof(volumeStr);
            }

            std::string pitchStr = ExtractYamlValue(propertiesBlock, "pitch");
            if (!pitchStr.empty()) {
                entity.audio.pitch = std::stof(pitchStr);
            }

            std::string loopStr = ExtractYamlValue(propertiesBlock, "loop");
            if (!loopStr.empty()) {
                entity.audio.loop = (loopStr == "true");
            }

            std::string playOnStartStr = ExtractYamlValue(propertiesBlock, "playOnStart");
            if (!playOnStartStr.empty()) {
                entity.audio.playOnStart = (playOnStartStr == "true");
            }

            // Parse 3D spatial audio properties
            std::string spatialBlendStr = ExtractYamlValue(propertiesBlock, "spatialBlend");
            if (!spatialBlendStr.empty()) {
                entity.audio.spatialBlend = std::stof(spatialBlendStr);
            }

            std::string minDistanceStr = ExtractYamlValue(propertiesBlock, "minDistance");
            if (!minDistanceStr.empty()) {
                entity.audio.minDistance = std::stof(minDistanceStr);
            }

            std::string maxDistanceStr = ExtractYamlValue(propertiesBlock, "maxDistance");
            if (!maxDistanceStr.empty()) {
                entity.audio.maxDistance = std::stof(maxDistanceStr);
            }

            // Parse rolloff mode
            std::string rolloffModeStr = ExtractYamlValue(propertiesBlock, "rolloffMode");
            if (!rolloffModeStr.empty()) {
                if (rolloffModeStr == "Linear") {
                    entity.audio.rolloffMode = AudioComponent::RolloffMode::Linear;
                } else if (rolloffModeStr == "Logarithmic") {
                    entity.audio.rolloffMode = AudioComponent::RolloffMode::Logarithmic;
                } else if (rolloffModeStr == "Custom") {
                    entity.audio.rolloffMode = AudioComponent::RolloffMode::Custom;
                } else {
                    entity.audio.rolloffMode = AudioComponent::RolloffMode::Linear; // Default
                }
            }

            // Parse advanced audio properties
            std::string dopplerLevelStr = ExtractYamlValue(propertiesBlock, "dopplerLevel");
            if (!dopplerLevelStr.empty()) {
                entity.audio.dopplerLevel = std::stof(dopplerLevelStr);
            }

            std::string spreadStr = ExtractYamlValue(propertiesBlock, "spread");
            if (!spreadStr.empty()) {
                entity.audio.spread = std::stof(spreadStr);
            }

            std::string reverbZoneMixStr = ExtractYamlValue(propertiesBlock, "reverbZoneMix");
            if (!reverbZoneMixStr.empty()) {
                entity.audio.reverbZoneMix = std::stof(reverbZoneMixStr);
            }

            // Parse playback properties
            std::string priorityStr = ExtractYamlValue(propertiesBlock, "priority");
            if (!priorityStr.empty()) {
                entity.audio.priority = std::stoi(priorityStr);
            }

            // Parse output routing
            std::string mixerGroupStr = ExtractYamlValue(propertiesBlock, "outputAudioMixerGroup");
            if (!mixerGroupStr.empty()) {
                entity.audio.outputAudioMixerGroup = mixerGroupStr;
            }

            // Parse audio metadata
            std::string audioNameStr = ExtractYamlValue(propertiesBlock, "audioName");
            if (!audioNameStr.empty()) {
                entity.audio.audioName = audioNameStr;
            }

        } else if (entity.type == GameObjectType::SPAWN_POINT) {
            entity.spawnPoint.team = std::stoi(ExtractYamlValue(propertiesBlock, "team"));
            entity.spawnPoint.priority = std::stoi(ExtractYamlValue(propertiesBlock, "priority"));
            entity.spawnPoint.cooldownTime = std::stof(ExtractYamlValue(propertiesBlock, "cooldown"));
        } else if (entity.type == GameObjectType::STATIC_PROP) {
            // Parse generic properties for static props
            entity.properties = ParseProperties(propertiesBlock);
        }

        // Parse collider properties (can be attached to any entity)
        std::string colliderBlock = ExtractYamlBlock(propertiesBlock, "collider");
        if (!colliderBlock.empty()) {
            std::string sizeStr = ExtractYamlValue(colliderBlock, "size");
            if (!sizeStr.empty()) {
                entity.collidable.size = ParseVector3(sizeStr);
            }

            std::string collisionLayerStr = ExtractYamlValue(colliderBlock, "collisionLayer");
            if (!collisionLayerStr.empty()) {
                // Parse collision layer by name
                if (collisionLayerStr == "PLAYER") entity.collidable.collisionLayer = LAYER_PLAYER;
                else if (collisionLayerStr == "ENEMY") entity.collidable.collisionLayer = LAYER_ENEMY;
                else if (collisionLayerStr == "WORLD") entity.collidable.collisionLayer = LAYER_WORLD;
                else if (collisionLayerStr == "PROJECTILE") entity.collidable.collisionLayer = LAYER_PROJECTILE;
                else if (collisionLayerStr == "PICKUP") entity.collidable.collisionLayer = LAYER_PICKUP;
                else if (collisionLayerStr == "DEBRIS") entity.collidable.collisionLayer = LAYER_DEBRIS;
                else entity.collidable.collisionLayer = LAYER_DEBRIS; // Default
            }

            std::string collisionMaskStr = ExtractYamlValue(colliderBlock, "collisionMask");
            if (!collisionMaskStr.empty()) {
                // For now, keep default mask - could parse array of layer names
                LOG_WARNING("collisionMask parsing not fully implemented, using defaults");
            }

            std::string isStaticStr = ExtractYamlValue(colliderBlock, "isStatic");
            if (!isStaticStr.empty()) {
                entity.collidable.isStatic = (isStaticStr == "true");
            }

            std::string isTriggerStr = ExtractYamlValue(colliderBlock, "isTrigger");
            if (!isTriggerStr.empty()) {
                entity.collidable.isTrigger = (isTriggerStr == "true");
            }
        }

        // Parse mesh properties (can be attached to any entity)
        std::string meshBlock = ExtractYamlBlock(propertiesBlock, "mesh");
        if (!meshBlock.empty()) {
            std::string typeStr = ExtractYamlValue(meshBlock, "type");
            if (!typeStr.empty()) {
                if (typeStr == "model") {
                    entity.mesh.type = decltype(entity.mesh)::MeshType::MODEL;
                } else if (typeStr == "primitive") {
                    entity.mesh.type = decltype(entity.mesh)::MeshType::PRIMITIVE;
                } else if (typeStr == "composite") {
                    entity.mesh.type = decltype(entity.mesh)::MeshType::COMPOSITE;
                }
            }

            std::string modelStr = ExtractYamlValue(meshBlock, "model");
            if (!modelStr.empty()) {
                entity.mesh.modelPath = modelStr;
            }

            std::string shapeStr = ExtractYamlValue(meshBlock, "shape");
            if (!shapeStr.empty()) {
                entity.mesh.primitiveShape = shapeStr;
            }

            std::string sizeStr = ExtractYamlValue(meshBlock, "size");
            if (!sizeStr.empty()) {
                entity.mesh.size = ParseVector3(sizeStr);
            }

            std::string subdivisionsStr = ExtractYamlValue(meshBlock, "subdivisions");
            if (!subdivisionsStr.empty()) {
                entity.mesh.subdivisions = std::stoi(subdivisionsStr);
            }

            std::string materialStr = ExtractYamlValue(meshBlock, "material");
            if (!materialStr.empty()) {
                entity.mesh.materialId = std::stoi(materialStr);
            }

            std::string castShadowsStr = ExtractYamlValue(meshBlock, "castShadows");
            if (!castShadowsStr.empty()) {
                entity.mesh.castShadows = (castShadowsStr == "true");
            }

            std::string receiveShadowsStr = ExtractYamlValue(meshBlock, "receiveShadows");
            if (!receiveShadowsStr.empty()) {
                entity.mesh.receiveShadows = (receiveShadowsStr == "true");
            }

            std::string meshNameStr = ExtractYamlValue(meshBlock, "meshName");
            if (!meshNameStr.empty()) {
                entity.mesh.meshName = meshNameStr;
            }
        }

        // Parse sprite properties (can be attached to any entity)
        std::string spriteBlock = ExtractYamlBlock(propertiesBlock, "sprite");
        if (!spriteBlock.empty()) {
            std::string textureStr = ExtractYamlValue(spriteBlock, "texture");
            if (!textureStr.empty()) {
                entity.sprite.texturePath = textureStr;
            }

            std::string sizeStr = ExtractYamlValue(spriteBlock, "size");
            if (!sizeStr.empty()) {
                Vector2 sizeVec = ParseVector2(sizeStr);
                entity.sprite.size = sizeVec;
            }

            std::string pivotStr = ExtractYamlValue(spriteBlock, "pivot");
            if (!pivotStr.empty()) {
                Vector2 pivotVec = ParseVector2(pivotStr);
                entity.sprite.pivot = pivotVec;
            }

            std::string pixelsPerUnitStr = ExtractYamlValue(spriteBlock, "pixelsPerUnit");
            if (!pixelsPerUnitStr.empty()) {
                entity.sprite.pixelsPerUnit = std::stof(pixelsPerUnitStr);
            }

            std::string colorStr = ExtractYamlValue(spriteBlock, "color");
            if (!colorStr.empty()) {
                entity.sprite.color = ParseColor(colorStr);
            }

            std::string animatedStr = ExtractYamlValue(spriteBlock, "animated");
            if (!animatedStr.empty()) {
                entity.sprite.animated = (animatedStr == "true");
            }

            // Parse animation frames if present
            std::string animationBlock = ExtractYamlBlock(spriteBlock, "animation");
            if (!animationBlock.empty()) {
                std::string framesBlock = ExtractYamlBlock(animationBlock, "frames");
                if (!framesBlock.empty()) {
                    auto frameItems = StringUtils::Split(framesBlock, '-');
                    for (const auto& frameItem : frameItems) {
                        std::string trimmed = StringUtils::Trim(frameItem);
                        if (!trimmed.empty() && trimmed[0] == '"') {
                            // Extract quoted string
                            size_t start = trimmed.find('"');
                            size_t end = trimmed.find('"', start + 1);
                            if (start != std::string::npos && end != std::string::npos) {
                                entity.sprite.animationFrames.push_back(trimmed.substr(start + 1, end - start - 1));
                            }
                        }
                    }
                }

                std::string fpsStr = ExtractYamlValue(animationBlock, "framesPerSecond");
                if (!fpsStr.empty()) {
                    entity.sprite.framesPerSecond = std::stof(fpsStr);
                }

                std::string loopStr = ExtractYamlValue(animationBlock, "loop");
                if (!loopStr.empty()) {
                    entity.sprite.animationLoop = (loopStr == "true");
                }
            }
        }

        // Parse material properties (can be attached to any entity)
        std::string materialBlock = ExtractYamlBlock(propertiesBlock, "material");
        if (!materialBlock.empty()) {
            std::string colorModeStr = ExtractYamlValue(materialBlock, "colorMode");
            if (!colorModeStr.empty()) {
                if (colorModeStr == "solid") {
                    entity.material.colorMode = decltype(entity.material)::ColorMode::SOLID;
                } else if (colorModeStr == "gradient") {
                    entity.material.colorMode = decltype(entity.material)::ColorMode::GRADIENT;
                } else if (colorModeStr == "vertex") {
                    entity.material.colorMode = decltype(entity.material)::ColorMode::VERTEX;
                }
            }

            std::string diffuseColorStr = ExtractYamlValue(materialBlock, "diffuseColor");
            if (!diffuseColorStr.empty()) {
                entity.material.diffuseColor = ParseColor(diffuseColorStr);
            }

            std::string gradientStartStr = ExtractYamlValue(materialBlock, "gradientStart");
            if (!gradientStartStr.empty()) {
                entity.material.gradientStart = ParseColor(gradientStartStr);
            }

            std::string gradientEndStr = ExtractYamlValue(materialBlock, "gradientEnd");
            if (!gradientEndStr.empty()) {
                entity.material.gradientEnd = ParseColor(gradientEndStr);
            }

            std::string gradientDirectionStr = ExtractYamlValue(materialBlock, "gradientDirection");
            if (!gradientDirectionStr.empty()) {
                entity.material.gradientDirection = ParseVector3(gradientDirectionStr);
            }

            std::string shininessStr = ExtractYamlValue(materialBlock, "shininess");
            if (!shininessStr.empty()) {
                entity.material.shininess = std::stof(shininessStr);
            }
        }
    }

    return entity;
}

std::unordered_map<std::string, std::any> MapLoader::ParseProperties(const std::string& propertiesYaml) {
    std::unordered_map<std::string, std::any> properties;
    std::stringstream ss(propertiesYaml);
    std::string line;

    while (std::getline(ss, line)) {
        std::string trimmed = StringUtils::Trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue; // Skip empty lines and comments

        // Look for key: value patterns
        size_t colonPos = trimmed.find(':');
        if (colonPos != std::string::npos) {
            std::string key = StringUtils::Trim(trimmed.substr(0, colonPos));
            std::string value = StringUtils::Trim(trimmed.substr(colonPos + 1));

            // Remove quotes if present
            if (!value.empty() && value[0] == '"' && value.back() == '"') {
                value = value.substr(1, value.size() - 2);
            }

            // Try to parse the value
            try {
                // Check if it's a number
                if (value.find('.') != std::string::npos) {
                    // Float
                    properties[key] = std::stof(value);
                } else if (value.find_first_not_of("0123456789-") == std::string::npos && !value.empty()) {
                    // Integer
                    properties[key] = std::stoi(value);
                } else {
                    // String
                    properties[key] = value;
                }
            } catch (const std::exception&) {
                // If parsing fails, store as string
                properties[key] = value;
            }
        }
    }

    return properties;
}

Vector3 MapLoader::ParseVector3(const std::string& vecStr) {
    Vector3 result = {0, 0, 0};

    // Remove brackets and split by comma
    std::string cleanStr = vecStr;
    cleanStr.erase(std::remove(cleanStr.begin(), cleanStr.end(), '['), cleanStr.end());
    cleanStr.erase(std::remove(cleanStr.begin(), cleanStr.end(), ']'), cleanStr.end());

    auto parts = StringUtils::Split(cleanStr, ',');
    if (parts.size() >= 3) {
        result.x = std::stof(StringUtils::Trim(parts[0]));
        result.y = std::stof(StringUtils::Trim(parts[1]));
        result.z = std::stof(StringUtils::Trim(parts[2]));
    }

    return result;
}

Quaternion MapLoader::ParseQuaternion(const std::string& quatStr) {
    Quaternion result = {0, 0, 0, 1}; // Default identity quaternion

    // Remove brackets and split by comma
    std::string cleanStr = quatStr;
    cleanStr.erase(std::remove(cleanStr.begin(), cleanStr.end(), '['), cleanStr.end());
    cleanStr.erase(std::remove(cleanStr.begin(), cleanStr.end(), ']'), cleanStr.end());

    auto parts = StringUtils::Split(cleanStr, ',');
    if (parts.size() >= 4) {
        result.x = std::stof(StringUtils::Trim(parts[0]));
        result.y = std::stof(StringUtils::Trim(parts[1]));
        result.z = std::stof(StringUtils::Trim(parts[2]));
        result.w = std::stof(StringUtils::Trim(parts[3]));
    }

    return result;
}

Color MapLoader::ParseColor(const std::string& colorStr) {
    Color result = {255, 255, 255, 255};

    // Remove brackets and split by comma
    std::string cleanStr = colorStr;
    cleanStr.erase(std::remove(cleanStr.begin(), cleanStr.end(), '['), cleanStr.end());
    cleanStr.erase(std::remove(cleanStr.begin(), cleanStr.end(), ']'), cleanStr.end());

    auto parts = StringUtils::Split(cleanStr, ',');
    if (parts.size() >= 3) {
        result.r = static_cast<unsigned char>(std::stoi(StringUtils::Trim(parts[0])));
        result.g = static_cast<unsigned char>(std::stoi(StringUtils::Trim(parts[1])));
        result.b = static_cast<unsigned char>(std::stoi(StringUtils::Trim(parts[2])));
        if (parts.size() >= 4) {
            result.a = static_cast<unsigned char>(std::stoi(StringUtils::Trim(parts[3])));
        }
    }

    return result;
}

Vector2 MapLoader::ParseVector2(const std::string& vecStr) {
    Vector2 result = {0, 0};

    // Remove brackets and split by comma
    std::string cleanStr = vecStr;
    cleanStr.erase(std::remove(cleanStr.begin(), cleanStr.end(), '['), cleanStr.end());
    cleanStr.erase(std::remove(cleanStr.begin(), cleanStr.end(), ']'), cleanStr.end());

    auto parts = StringUtils::Split(cleanStr, ',');
    if (parts.size() >= 2) {
        result.x = std::stof(StringUtils::Trim(parts[0]));
        result.y = std::stof(StringUtils::Trim(parts[1]));
    }

    return result;
}

void MapLoader::GenerateDefaultUVs(Face& face) {
    if (face.vertices.size() < 3) return;

    LOG_DEBUG("GenerateDefaultUVs: Generating UVs for face with " + std::to_string(face.vertices.size()) + " vertices");

    // Generate planar UV mapping based on the face's normal
    // Project vertices onto the best fitting plane and create UV coordinates

    // Find the dominant axis based on normal
    Vector3 absNormal = {std::abs(face.normal.x), std::abs(face.normal.y), std::abs(face.normal.z)};
    LOG_DEBUG("GenerateDefaultUVs: normal=(" + std::to_string(face.normal.x) + "," + std::to_string(face.normal.y) + "," + std::to_string(face.normal.z) + ")");

    face.uvs.clear();
    face.uvs.reserve(face.vertices.size());

    if (absNormal.y >= absNormal.x && absNormal.y >= absNormal.z) {
        // Y-dominant face (horizontal), use XZ plane
        LOG_DEBUG("GenerateDefaultUVs: Using XZ plane mapping (horizontal face)");
        for (const auto& vertex : face.vertices) {
            face.uvs.push_back({vertex.x * 0.1f, vertex.z * 0.1f});
        }
    } else if (absNormal.x >= absNormal.z) {
        // X-dominant face, use YZ plane
        LOG_DEBUG("GenerateDefaultUVs: Using YZ plane mapping (X-dominant face)");
        for (const auto& vertex : face.vertices) {
            face.uvs.push_back({vertex.y * 0.1f, vertex.z * 0.1f});
        }
    } else {
        // Z-dominant face, use XY plane
        LOG_DEBUG("GenerateDefaultUVs: Using XY plane mapping (Z-dominant face)");
        for (const auto& vertex : face.vertices) {
            face.uvs.push_back({vertex.x * 0.1f, vertex.y * 0.1f});
        }
    }

    LOG_DEBUG("GenerateDefaultUVs: Generated " + std::to_string(face.uvs.size()) + " UV coordinates");
    if (!face.uvs.empty()) {
        LOG_DEBUG("GenerateDefaultUVs: First UV: (" + std::to_string(face.uvs[0].x) + "," + std::to_string(face.uvs[0].y) + ")");
    }
}

// YAML parsing utility methods
std::string MapLoader::ExtractYamlValue(const std::string& yaml, const std::string& key) {
    std::stringstream ss(yaml);
    std::string line;
    std::string searchKey = key + ":";

    while (std::getline(ss, line)) {
        std::string trimmed = StringUtils::Trim(line);
        if (trimmed.find(searchKey) == 0) {
            // Extract value after colon
            size_t colonPos = trimmed.find(':');
            if (colonPos != std::string::npos) {
                return TrimYamlValue(trimmed.substr(colonPos + 1));
            }
        }
    }

    return "";
}

std::string MapLoader::ExtractYamlBlock(const std::string& yaml, const std::string& key) {
    std::stringstream ss(yaml);
    std::string line;
    std::string result;
    std::string searchKey = key + ":";
    bool inBlock = false;
    int baseIndent = -1;

    LOG_DEBUG("ExtractYamlBlock: Looking for key '" + searchKey + "' in YAML");
    if (yaml.length() < 300) {
        LOG_DEBUG("ExtractYamlBlock: Input YAML content:\n" + yaml);
    } else {
        LOG_DEBUG("ExtractYamlBlock: Input YAML content (first 300 chars):\n" + yaml.substr(0, 300));
    }

    while (std::getline(ss, line)) {
        std::string trimmed = StringUtils::Trim(line);
        int lineIndent = GetYamlIndentation(line);

        LOG_DEBUG("ExtractYamlBlock: Line indent " + std::to_string(lineIndent) + ": '" + trimmed + "' (searching for '" + searchKey + "')");
        LOG_DEBUG("ExtractYamlBlock: Raw line: '" + line + "'");

        // Check both exact match and prefix match for robustness
        if (trimmed == searchKey || trimmed.find(searchKey) == 0) {
            inBlock = true;
            baseIndent = lineIndent;
            LOG_DEBUG("ExtractYamlBlock: Found key '" + searchKey + "' at indent " + std::to_string(baseIndent) + ", starting block collection");
            continue;
        }

        if (inBlock) {
            // Check if this is another top-level key (ends with : and has same or lower indent as original file, but not a list item)
            bool isTopLevelKey = (trimmed.find(':') != std::string::npos) && (lineIndent <= 0) && (trimmed != searchKey) && (trimmed[0] != '-');

            if (isTopLevelKey) {
                LOG_DEBUG("ExtractYamlBlock: Found another top-level key, ending block: '" + trimmed + "'");
                break; // End of block
            } else if (lineIndent >= baseIndent || trimmed.empty() || trimmed[0] == '-') {
                result += line + "\n";
                LOG_DEBUG("ExtractYamlBlock: Added line to result: '" + trimmed + "'");
            } else if (lineIndent < baseIndent && !trimmed.empty() && trimmed[0] != '-') {
                LOG_DEBUG("ExtractYamlBlock: End of block reached at lower indent " + std::to_string(lineIndent) + ", line: '" + line + "'");
                break; // End of block
            }
        }
    }

    LOG_DEBUG("ExtractYamlBlock: Returning result of length " + std::to_string(result.length()));
    return result;
}

std::vector<std::string> MapLoader::ExtractYamlList(const std::string& yaml, const std::string& key) {
    std::vector<std::string> result;
    std::stringstream ss(yaml);
    std::string line;
    std::string currentItem;
    int baseIndent = -1;
    bool inList = false;

    while (std::getline(ss, line)) {
        std::string trimmed = StringUtils::Trim(line);

        // Skip empty lines
        if (trimmed.empty()) continue;

        // Check if this is a list item (starts with -) at the correct indentation level
        int currentIndent = GetYamlIndentation(line);
        if (trimmed[0] == '-' && (baseIndent == -1 || currentIndent == baseIndent)) {
            // If we were building a previous item, save it
            if (!currentItem.empty()) {
                result.push_back(currentItem);
                currentItem.clear();
            }

            // Start new item
            currentItem = trimmed.substr(1); // Remove the -
            currentItem = StringUtils::Trim(currentItem);

            // Get base indentation for this list item
            if (baseIndent == -1) {
                baseIndent = currentIndent;
            }

            inList = true;
        } else if (inList) {
            // Continuation of current list item
            int currentIndent = GetYamlIndentation(line);
            if (currentIndent > baseIndent || (currentIndent == baseIndent && trimmed[0] != '-')) {
                // This line belongs to the current item
                currentItem += "\n" + line;
            } else if (currentIndent == baseIndent && trimmed[0] == '-') {
                // Start of a new list item
                if (!currentItem.empty()) {
                    result.push_back(currentItem);
                    currentItem.clear();
                }
                currentItem = trimmed.substr(1);
                currentItem = StringUtils::Trim(currentItem);
            } else if (currentIndent < baseIndent) {
                // End of list
                if (!currentItem.empty()) {
                    result.push_back(currentItem);
                    currentItem.clear();
                }
                break;
            }
        }
    }

    // Don't forget the last item
    if (!currentItem.empty()) {
        result.push_back(currentItem);
    }

    return result;
}

int MapLoader::GetYamlIndentation(const std::string& line) const {
    int indent = 0;
    for (char c : line) {
        if (c == ' ') {
            indent++;
        } else if (c == '\t') {
            indent += 4; // Assume 4 spaces per tab
        } else {
            break;
        }
    }
    return indent;
}

std::string MapLoader::TrimYamlValue(const std::string& value) const {
    std::string result = StringUtils::Trim(value);

    // Remove quotes if present
    if (result.size() >= 2 && result.front() == '"' && result.back() == '"') {
        result = result.substr(1, result.size() - 2);
    } else if (result.size() >= 2 && result.front() == '\'' && result.back() == '\'') {
        result = result.substr(1, result.size() - 2);
    }

    return result;
}
