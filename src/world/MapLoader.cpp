#include "MapLoader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include "../utils/Logger.h"

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

    LOG_INFO("Map parsing completed successfully. Surfaces: " +
              std::to_string(mapData.surfaces.size()) +
              ", Textures: " + std::to_string(mapData.textures.size()));
    return mapData;
}


bool MapLoader::ParseMapFile(const std::string& content, MapData& mapData) {
    std::stringstream ss(content);
    std::string line;

    while (std::getline(ss, line)) {
        line = Trim(line);

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;

        if (line.find("name:") == 0) {
            mapData.name = Trim(line.substr(5));
        } else if (line.find("sky:") == 0) {
            // Parse sky color (simple format: r,g,b)
            std::string colorStr = Trim(line.substr(4));
            auto parts = Split(colorStr, ',');
            if (parts.size() == 3) {
                mapData.skyColor.r = static_cast<unsigned char>(std::stoi(parts[0]));
                mapData.skyColor.g = static_cast<unsigned char>(std::stoi(parts[1]));
                mapData.skyColor.b = static_cast<unsigned char>(std::stoi(parts[2]));
            }
        } else if (line.find("texture:") == 0) {
            ParseTexture(line, mapData);
        } else if (line.find("surface:") == 0) {
            Surface surface = ParseSurface(line);
            if (surface.start.x != 0 || surface.start.z != 0 ||
                surface.end.x != 0 || surface.end.z != 0) {
                mapData.surfaces.push_back(surface);
            }
        } else if (line.find("floor:") == 0) {
            mapData.floorHeight = std::stof(Trim(line.substr(6)));
        } else if (line.find("ceiling:") == 0) {
            mapData.ceilingHeight = std::stof(Trim(line.substr(8)));
        }
    }

    return !mapData.surfaces.empty();
}


Surface MapLoader::ParseSurface(const std::string& line) {
    // Simple surface format: surface: x1,y1,z1 x2,y2,z2 height textureIndex color
    Surface surface;
    std::string data = Trim(line.substr(8));
    std::vector<std::string> parts = Split(data, ' ');

    if (parts.size() >= 7) {
        // Parse start position
        auto startParts = Split(parts[0], ',');
        if (startParts.size() == 3) {
            surface.start.x = std::stof(startParts[0]);
            surface.start.y = std::stof(startParts[1]);
            surface.start.z = std::stof(startParts[2]);
        }

        // Parse end position
        auto endParts = Split(parts[1], ',');
        if (endParts.size() == 3) {
            surface.end.x = std::stof(endParts[0]);
            surface.end.y = std::stof(endParts[1]);
            surface.end.z = std::stof(endParts[2]);
        }

        surface.height = std::stof(parts[2]);
        surface.textureIndex = std::stoi(parts[3]);

        // Parse color
        auto colorParts = Split(parts[4], ',');
        if (colorParts.size() == 3) {
            surface.color.r = static_cast<unsigned char>(std::stoi(colorParts[0]));
            surface.color.g = static_cast<unsigned char>(std::stoi(colorParts[1]));
            surface.color.b = static_cast<unsigned char>(std::stoi(colorParts[2]));
        }
    }

    return surface;
}

bool MapLoader::ParseTexture(const std::string& line, MapData& mapData) {
    // Simple texture format: texture: name index
    std::string data = Trim(line.substr(9));
    std::vector<std::string> parts = Split(data, ' ');

    if (parts.size() >= 2) {
        std::string name = parts[0];
        int index = std::stoi(parts[1]);
        mapData.textures.emplace_back(name, index);
        return true;
    }

    return false;
}


std::string MapLoader::Trim(const std::string& str) const {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

std::vector<std::string> MapLoader::Split(const std::string& str, char delimiter) const {
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string token;

    while (std::getline(ss, token, delimiter)) {
        result.push_back(Trim(token));
    }

    return result;
}
