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

    LOG_INFO("Map parsing completed successfully. Faces: " +
              std::to_string(mapData.faces.size()) +
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
            Face face = ParseFace(line);
            if (face.vertices.size() >= 3) {
                mapData.faces.push_back(face);
            }
        } else if (line.find("floor:") == 0) {
            mapData.floorHeight = std::stof(Trim(line.substr(6)));
        } else if (line.find("ceiling:") == 0) {
            mapData.ceilingHeight = std::stof(Trim(line.substr(8)));
        }
    }

    // If no brushes provided in file, aggregate faces into a single brush to preserve unflattened data
    if (!mapData.faces.empty() && mapData.brushes.empty()) {
        Brush brush;
        brush.faces = mapData.faces;
        brush.RecalculateBounds();
        mapData.brushes.push_back(brush);
    }

    return !mapData.faces.empty();
}


Face MapLoader::ParseFace(const std::string& line) {
    // Legacy surface format converted to a Face quad:
    // surface: x1,y1,z1 x2,y2,z2 height textureIndex color
    // If height ~ 0 -> horizontal quad between start/end (floor/ceiling by y sign)
    // If height != 0 -> vertical wall quad extruded from start/end up by height
    std::string data = Trim(line.substr(8));
    std::vector<std::string> parts = Split(data, ' ');

    Face face;
    if (parts.size() >= 7) {
        // Parse positions
        auto sP = Split(parts[0], ',');
        auto eP = Split(parts[1], ',');
        if (sP.size() == 3 && eP.size() == 3) {
            Vector3 start{std::stof(sP[0]), std::stof(sP[1]), std::stof(sP[2])};
            Vector3 end{std::stof(eP[0]), std::stof(eP[1]), std::stof(eP[2])};
            float height = std::stof(parts[2]);
            int texIndex = std::stoi(parts[3]);
            auto cP = Split(parts[4], ',');
            Color tint = {255,255,255,255};
            if (cP.size() == 3) {
                tint.r = static_cast<unsigned char>(std::stoi(cP[0]));
                tint.g = static_cast<unsigned char>(std::stoi(cP[1]));
                tint.b = static_cast<unsigned char>(std::stoi(cP[2]));
            }

            face.materialId = texIndex;
            face.tint = tint;

            if (fabsf(height) < 0.001f) {
                // Horizontal quad from start/end at same Y
                float y = start.y; // assume start/end.y are identical in legacy
                Vector3 p1{start.x, y, start.z};
                Vector3 p2{end.x,   y, start.z};
                Vector3 p3{end.x,   y, end.z};
                Vector3 p4{start.x, y, end.z};
                face.vertices = {p1,p2,p3,p4};
            } else {
                // Vertical wall quad
                float bottomY = start.y;
                float topY = bottomY + height;
                Vector3 bottomLeft{start.x, bottomY, start.z};
                Vector3 bottomRight{end.x,   bottomY, end.z};
                Vector3 topRight{end.x,   topY, end.z};
                Vector3 topLeft{start.x, topY, start.z};
                face.vertices = {bottomLeft, bottomRight, topRight, topLeft};
            }

            face.RecalculateNormal();
        }
    }
    return face;
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
