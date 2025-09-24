#pragma once

#include "Brush.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <any>
#include <memory>
#include "raylib.h"

// Include component headers for enum types
#include "../ecs/Components/GameObject.h"
#include "../ecs/Components/LightComponent.h"
#include "../ecs/Components/AudioComponent.h"
#include "../ecs/Components/Collidable.h"
#include "../ecs/Components/MeshComponent.h"
#include "../ecs/Components/Sprite.h"
#include "../ecs/Components/EnemyComponent.h"
#include "../ecs/Components/TriggerComponent.h"
#include "../ecs/Components/SpawnPointComponent.h"

// Represents a loaded texture with its properties
struct TextureInfo {
    Texture2D texture;
    std::string name;
    int index;

    TextureInfo() : index(-1) {}
    TextureInfo(const std::string& n, int idx) : name(n), index(idx) {}
};

// Represents a parsed material with all its properties
struct MaterialInfo {
    int id = -1;
    std::string name;
    std::string type = "BASIC"; // BASIC, PBR, UNLIT, EMISSIVE, TRANSPARENT

    // Basic material properties
    Color diffuseColor = WHITE;
    Color specularColor = WHITE;
    float shininess = 32.0f;
    float alpha = 1.0f;

    // PBR properties
    float roughness = 0.5f;
    float metallic = 0.0f;
    float ao = 1.0f;

    // Emission properties
    Color emissiveColor = BLACK;
    float emissiveIntensity = 1.0f;

    // Texture maps
    std::string diffuseMap;
    std::string normalMap;
    std::string specularMap;
    std::string roughnessMap;
    std::string metallicMap;
    std::string aoMap;
    std::string emissiveMap;

    // Rendering flags
    bool doubleSided = false;
    bool depthWrite = true;
    bool depthTest = true;
    bool castShadows = true;

    MaterialInfo() = default;
    MaterialInfo(int materialId, const std::string& materialName)
        : id(materialId), name(materialName) {}
};

// Entity definition for map loading
struct EntityDefinition {
    uint32_t id = 0;
    std::string className;
    std::string name;
    GameObjectType type = GameObjectType::UNKNOWN;

    // Transform data
    Vector3 position = {0, 0, 0};
    Vector3 scale = {1, 1, 1};
    Quaternion rotation = {0, 0, 0, 1};

    // Properties (key-value pairs)
    std::unordered_map<std::string, std::any> properties;

    // Component-specific data - using actual component structures
    LightComponent light;

    struct {
        EnemyType type = EnemyType::BASIC;
        float health = 100.0f;
        float damage = 10.0f;
        float moveSpeed = 5.0f;
        int team = 0;
    } enemy;

    struct {
        TriggerType type = TriggerType::BOX;
        Vector3 size = {1, 1, 1};
        float radius = 1.0f;
        float height = 1.0f;
        int maxActivations = -1;
    } trigger;

    struct {
        SpawnPointType type = SpawnPointType::PLAYER;
        int team = 0;
        int priority = 1;
        float cooldownTime = 5.0f;
    } spawnPoint;

    AudioComponent audio;

    struct {
        Vector3 size = {1.0f, 1.0f, 1.0f};
        uint32_t collisionLayer = LAYER_DEBRIS;
        uint32_t collisionMask = LAYER_WORLD | LAYER_PLAYER | LAYER_DEBRIS;
        bool isStatic = false;
        bool isTrigger = false;
    } collidable;

    struct {
        enum class MeshType { MODEL, PRIMITIVE, COMPOSITE };
        MeshType type = MeshType::PRIMITIVE;
        std::string modelPath;
        std::string primitiveShape = "cube";
        Vector3 size = {1.0f, 1.0f, 1.0f};
        int subdivisions = 8;
        int materialId = 0;
        bool castShadows = true;
        bool receiveShadows = true;
        std::string meshName = "default";
    } mesh;

    struct {
        std::string texturePath;
        Vector2 size = {1.0f, 1.0f};
        Vector2 pivot = {0.5f, 0.5f};
        float pixelsPerUnit = 100.0f;
        Color color = WHITE;
        bool animated = false;
        std::vector<std::string> animationFrames;
        float framesPerSecond = 12.0f;
        bool animationLoop = true;
    } sprite;

    struct {
        enum class ColorMode { SOLID, GRADIENT, VERTEX };
        ColorMode colorMode = ColorMode::SOLID;
        Color diffuseColor = WHITE;
        Color gradientStart = WHITE;
        Color gradientEnd = BLACK;
        Vector3 gradientDirection = {0.0f, 1.0f, 0.0f};
        float shininess = 32.0f;
    } material;
};

// Simple map format representation
struct MapData {
    std::string name;
    std::vector<Face> faces;
    std::vector<Brush> brushes;
    std::vector<MaterialInfo> materials;
    std::vector<std::unique_ptr<EntityDefinition>> entities;
    Color skyColor;
    float floorHeight;
    float ceilingHeight;

    MapData() : skyColor(SKYBLUE), floorHeight(0.0f), ceilingHeight(8.0f) {}
};

// Loads and parses .map files into raw MapData structs
// This class is solely responsible for parsing .map files and returning
// raw, unprocessed data structures. All processing (BSP building, texture loading,
// entity creation) is handled by other systems.
class MapLoader {
public:
    MapLoader();
    ~MapLoader() = default;

    // Parse a map file into raw MapData
    // mapPath: Path to the .map file
    // Returns: Raw MapData struct, empty if file not found or parsing failed
    MapData LoadMap(const std::string& mapPath);

private:
    // Parse a .map file format
    // content: File content as string
    // mapData: Output map data
    // Returns: True if parsing was successful
    bool ParseMapFile(const std::string& content, MapData& mapData);


    // YAML map format parsing (for development and editor use)
    // TODO: Add binary format support when editor is implemented for production builds
    bool ParseYamlMap(const std::string& content, MapData& mapData);
    bool ParseEntities(const std::string& yamlContent, MapData& mapData);
    bool ParseWorldGeometry(const std::string& worldYaml, MapData& mapData);
    bool ParseMaterials(const std::string& materialsYaml, MapData& mapData);
    bool ParseBrush(const std::string& brushYaml, MapData& mapData);
    bool ParseBrushFace(const std::string& faceYaml, MapData& mapData);
    EntityDefinition ParseEntity(const std::string& entityYaml, uint32_t id);
    std::unordered_map<std::string, std::any> ParseProperties(const std::string& propertiesYaml);
    Vector3 ParseVector3(const std::string& vecStr);
    Vector2 ParseVector2(const std::string& vecStr);
    Quaternion ParseQuaternion(const std::string& quatStr);
    Color ParseColor(const std::string& colorStr);
    void GenerateDefaultUVs(Face& face);

    // Utility methods for YAML parsing
    std::string ExtractYamlValue(const std::string& yaml, const std::string& key);
    std::string ExtractYamlBlock(const std::string& yaml, const std::string& key);
    std::vector<std::string> ExtractYamlList(const std::string& yaml, const std::string& key);
    int GetYamlIndentation(const std::string& line) const;
    std::string TrimYamlValue(const std::string& value) const;
};
