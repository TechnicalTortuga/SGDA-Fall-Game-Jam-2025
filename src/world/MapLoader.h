#pragma once

#include "Brush.h"
#include <string>
#include <vector>
#include <unordered_map>
#include "raylib.h"

// Represents a loaded texture with its properties
struct TextureInfo {
    Texture2D texture;
    std::string name;
    int index;

    TextureInfo() : index(-1) {}
    TextureInfo(const std::string& n, int idx) : name(n), index(idx) {}
};

// Simple map format representation
struct MapData {
    std::string name;
    std::vector<Face> faces;
    std::vector<Brush> brushes;
    std::vector<TextureInfo> textures;
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

    // Parse a face definition line (legacy "surface:" entries converted to faces)
    // line: Line from map file
    // Returns: Face object
    Face ParseFace(const std::string& line);

    // Parse texture definition line
    // line: Line from map file
    // mapData: Map data to add texture to
    // Returns: True if parsing was successful
    bool ParseTexture(const std::string& line, MapData& mapData);

    // Trim whitespace from string
    // str: String to trim
    // Returns: Trimmed string
    std::string Trim(const std::string& str) const;

    // Split string by delimiter
    // str: String to split
    // delimiter: Delimiter character
    // Returns: Vector of string parts
    std::vector<std::string> Split(const std::string& str, char delimiter) const;
};
