#include "ShaderSystem.h"
#include "../utils/Logger.h"
#include "../utils/PathUtils.h"
#include "../core/Engine.h"
#include "../ecs/Systems/LightSystem.h"
#include <filesystem>

namespace fs = std::filesystem;

ShaderSystem::ShaderSystem()
    : nextShaderId_(1), basicShaderId_(0), lightingShaderId_(0), pbrShaderId_(0), depthShaderId_(0), lightSystem_(nullptr) {
    LOG_INFO("ShaderSystem created");
}

ShaderSystem::~ShaderSystem() {
    LOG_INFO("ShaderSystem destroyed");
}

void ShaderSystem::Initialize() {
    LOG_INFO("Initializing ShaderSystem");
    
    // Set up shader directory relative to executable
    std::string exeDir = Utils::GetExecutableDir();
    shaderDirectory_ = exeDir + "/shaders/";
    
    // Create shader directory if it doesn't exist
    if (!fs::exists(shaderDirectory_)) {
        fs::create_directories(shaderDirectory_);
        LOG_INFO("Created shader directory: " + shaderDirectory_);
    }
    
    // Cache system references for performance
    lightSystem_ = Engine::GetInstance().GetSystem<LightSystem>();

    // Create default shaders
    basicShaderId_ = CreateDefaultBasicShader();
    if (basicShaderId_ == 0) {
        LOG_ERROR("Failed to create default basic shader");
    } else {
        LOG_INFO("Created default basic shader with ID: " + std::to_string(basicShaderId_));
    }

    // PBR shader can be created later when needed
    LOG_INFO("ShaderSystem initialized successfully");
}

void ShaderSystem::Update(float deltaTime) {
    // For now, no per-frame updates needed
    // Future: hot-reloading, shader parameter animation
}

void ShaderSystem::Shutdown() {
    LOG_INFO("ShaderSystem shutting down");
    
    // Unload all shaders
    for (auto& pair : shaders_) {
        if (pair.second && pair.second->shader.id > 0) {
            UnloadShader(pair.second->shader);
            LOG_DEBUG("Unloaded shader ID: " + std::to_string(pair.first));
        }
    }
    shaders_.clear();
    
    basicShaderId_ = 0;
    lightingShaderId_ = 0;
    pbrShaderId_ = 0;
    
    LOG_INFO("ShaderSystem shutdown complete");
}

uint32_t ShaderSystem::LoadShader(const std::string& vsPath, const std::string& fsPath, ShaderType type) {
    Shader shader;
    if (!LoadShaderFromFiles(vsPath, fsPath, shader)) {
        LOG_ERROR("Failed to load shader from " + vsPath + " and " + fsPath);
        return 0;
    }
    
    uint32_t shaderId = nextShaderId_++;
    auto shaderData = std::make_unique<ShaderData>();
    shaderData->shader = shader;
    shaderData->vertexPath = vsPath;
    shaderData->fragmentPath = fsPath;
    shaderData->type = type;
    shaderData->isDefault = false;
    
    SetupDefaultUniforms(*shaderData);
    
    shaders_[shaderId] = std::move(shaderData);
    
    LOG_INFO("Loaded shader ID " + std::to_string(shaderId) + " from " + vsPath + " and " + fsPath);
    return shaderId;
}

uint32_t ShaderSystem::GetOrCreateDefaultShader(ShaderType type) {
    switch (type) {
        case ShaderType::BASIC:
            if (basicShaderId_ == 0) {
                basicShaderId_ = CreateDefaultBasicShader();
            }
            return basicShaderId_;
            
        case ShaderType::LIGHTING:
            if (lightingShaderId_ == 0) {
                LOG_DEBUG("🔧 Creating lighting shader...");
                lightingShaderId_ = CreateDefaultLightingShader();
                LOG_DEBUG("🔧 Lighting shader created with ID: " + std::to_string(lightingShaderId_));
            }
            return lightingShaderId_;
            
        case ShaderType::PBR:
            if (pbrShaderId_ == 0) {
                pbrShaderId_ = CreateDefaultPBRShader();
            }
            return pbrShaderId_;

        default:
            LOG_WARNING("No default shader available for type " + std::to_string(static_cast<int>(type)));
            return basicShaderId_; // Fallback to basic
    }
}

uint32_t ShaderSystem::GetDepthShader() {
    if (depthShaderId_ == 0) {
        LOG_DEBUG("🔧 Creating depth shader...");
        std::string vsPath = GetShaderPath("depth/depth.vs");
        std::string fsPath = GetShaderPath("depth/depth.fs");

        Shader tempShader = ::LoadShader(vsPath.c_str(), fsPath.c_str());
        if (tempShader.id != 0) {
            auto shaderData = std::make_unique<ShaderData>();
            shaderData->shader = tempShader;
            shaderData->vertexPath = vsPath;
            shaderData->fragmentPath = fsPath;
            shaderData->type = ShaderType::DEPTH;
            shaderData->isDefault = true;

            depthShaderId_ = nextShaderId_++;
            SetupDefaultUniforms(*shaderData);
            shaders_[depthShaderId_] = std::move(shaderData);

            LOG_INFO("✅ DEPTH SHADER CREATED:");
            LOG_INFO("  Shader system ID: " + std::to_string(depthShaderId_));
            LOG_INFO("  Raylib shader ID: " + std::to_string(shaders_[depthShaderId_]->shader.id));
        } else {
            LOG_ERROR("❌ Failed to create depth shader");
            return 0;
        }
    }
    return depthShaderId_;
}

Shader* ShaderSystem::GetShader(uint32_t shaderId) {
    auto it = shaders_.find(shaderId);
    if (it != shaders_.end() && it->second) {
        return &it->second->shader;
    }
    return nullptr;
}

void ShaderSystem::ApplyShaderToModel(uint32_t shaderId, Model& model, int meshIndex) {
    Shader* shader = GetShader(shaderId);
    if (!shader) {
        LOG_WARNING("Failed to get shader ID " + std::to_string(shaderId) + " for model");
        return;
    }

    // For lighting shader, ensure uniforms are up to date before copying to materials
    if (shaderId == lightingShaderId_ && lightSystem_) {
        lightSystem_->UpdateShaderLights(*shader);
        lightSystem_->UpdateViewPosUniform(*shader);
        LOG_DEBUG("Updated lighting uniforms on shader before applying to model");
    }

    if (meshIndex == -1) {
        // Apply to all materials
        for (int i = 0; i < model.materialCount; i++) {
            model.materials[i].shader = *shader;
        }
        LOG_DEBUG("Applied shader " + std::to_string(shaderId) + " to all materials in model");
    } else if (meshIndex >= 0 && meshIndex < model.materialCount) {
        // Apply to specific material
        model.materials[meshIndex].shader = *shader;
        LOG_DEBUG("Applied shader " + std::to_string(shaderId) + " to material " + std::to_string(meshIndex));
    } else {
        LOG_WARNING("Invalid mesh index " + std::to_string(meshIndex) + " for model with " +
                   std::to_string(model.materialCount) + " materials");
    }
}

uint32_t ShaderSystem::CreateDefaultBasicShader() {
    LOG_INFO("🔧 CREATING default basic shader");
    
    // Try to load proper shader files first
    std::string vsPath = GetShaderPath("basic/basic.vs");
    std::string fsPath = GetShaderPath("basic/basic.fs");
    
    LOG_INFO("🔍 SHADER PATHS:");
    LOG_INFO("  Shader directory: " + shaderDirectory_);
    LOG_INFO("  Vertex shader: " + vsPath);
    LOG_INFO("  Fragment shader: " + fsPath);
    LOG_INFO("  VS file exists: " + std::string(fs::exists(vsPath) ? "YES" : "NO"));
    LOG_INFO("  FS file exists: " + std::string(fs::exists(fsPath) ? "YES" : "NO"));
    
    if (fs::exists(vsPath)) {
        LOG_INFO("  VS file size: " + std::to_string(fs::file_size(vsPath)) + " bytes");
    }
    if (fs::exists(fsPath)) {
        LOG_INFO("  FS file size: " + std::to_string(fs::file_size(fsPath)) + " bytes");
    }
    
    uint32_t shaderId = nextShaderId_++;
    auto shaderData = std::make_unique<ShaderData>();
    
    // Try to load from files
    bool loadedFromFiles = false;
    if (LoadShaderFromFiles(vsPath, fsPath, shaderData->shader)) {
        shaderData->vertexPath = vsPath;
        shaderData->fragmentPath = fsPath;
        loadedFromFiles = true;
        
        LOG_INFO("✅ SHADER FILES LOADED SUCCESSFULLY");
        LOG_INFO("  Vertex: " + vsPath);
        LOG_INFO("  Fragment: " + fsPath);
        LOG_INFO("  Shader ID: " + std::to_string(shaderData->shader.id));
    } else {
        // Fallback to Raylib's default shader
        LOG_WARNING("❌ SHADER FILES FAILED - using Raylib default shader");
        LOG_WARNING("  Attempted VS: " + vsPath);
        LOG_WARNING("  Attempted FS: " + fsPath);
        
        shaderData->shader = LoadShaderFromMemory(nullptr, nullptr);
        shaderData->vertexPath = "default_basic.vs";
        shaderData->fragmentPath = "default_basic.fs";
        
        if (shaderData->shader.id == 0) {
            LOG_ERROR("❌ FALLBACK SHADER CREATION FAILED");
            return 0;
        } else {
            LOG_INFO("✅ FALLBACK SHADER CREATED with ID: " + std::to_string(shaderData->shader.id));
        }
    }
    
    shaderData->type = ShaderType::BASIC;
    shaderData->isDefault = true;
    
    LOG_INFO("🎛️ SETTING UP shader uniforms");
    SetupDefaultUniforms(*shaderData);
    
    shaders_[shaderId] = std::move(shaderData);
    
    LOG_INFO("✅ BASIC SHADER CREATED:");
    LOG_INFO("  Shader system ID: " + std::to_string(shaderId));
    LOG_INFO("  Raylib shader ID: " + std::to_string(shaders_[shaderId]->shader.id));
    LOG_INFO("  Source: " + std::string(loadedFromFiles ? "custom files" : "Raylib default"));
    LOG_INFO("  Total shaders loaded: " + std::to_string(shaders_.size()));
    
    return shaderId;
}

uint32_t ShaderSystem::CreateDefaultLightingShader() {
    uint32_t shaderId = nextShaderId_++;
    auto shaderData = std::make_unique<ShaderData>();
    
    // Try to load custom lighting shaders with shadows (our working implementation)
    std::string vsPath = GetShaderPath("lighting/lighting.vs");
    std::string fsPath = GetShaderPath("lighting/lighting_shadows.fs");
    
    bool loadedFromFiles = false;
    if (LoadShaderFromFiles(vsPath, fsPath, shaderData->shader)) {
        shaderData->vertexPath = vsPath;
        shaderData->fragmentPath = fsPath;
        loadedFromFiles = true;
        
        LOG_INFO("✅ LIGHTING SHADER FILES LOADED SUCCESSFULLY");
        LOG_INFO("  Vertex: " + vsPath);
        LOG_INFO("  Fragment: " + fsPath);
        LOG_INFO("  Shader ID: " + std::to_string(shaderData->shader.id));
    } else {
        // Fallback to basic shader
        LOG_WARNING("❌ LIGHTING SHADER FILES FAILED - using basic shader fallback");
        LOG_WARNING("  Attempted VS: " + vsPath);
        LOG_WARNING("  Attempted FS: " + fsPath);
        
        // Just load the basic shader as fallback
        return CreateDefaultBasicShader();
    }
    
    shaderData->type = ShaderType::LIGHTING;
    shaderData->isDefault = true;
    
    LOG_INFO("🎛️ SETTING UP lighting shader uniforms");
    SetupDefaultUniforms(*shaderData);
    
    shaders_[shaderId] = std::move(shaderData);
    
    LOG_INFO("✅ LIGHTING SHADER CREATED:");
    LOG_INFO("  Shader system ID: " + std::to_string(shaderId));
    LOG_INFO("  Raylib shader ID: " + std::to_string(shaders_[shaderId]->shader.id));
    LOG_INFO("  Source: " + std::string(loadedFromFiles ? "custom files" : "fallback"));
    LOG_INFO("  Total shaders loaded: " + std::to_string(shaders_.size()));
    
    return shaderId;
}

uint32_t ShaderSystem::CreateDefaultPBRShader() {
    // For now, return basic shader as PBR fallback
    return CreateDefaultBasicShader();
}

std::string ShaderSystem::GetShaderPath(const std::string& filename) const {
    return shaderDirectory_ + filename;
}

bool ShaderSystem::LoadShaderFromFiles(const std::string& vsPath, const std::string& fsPath, Shader& outShader) {
    // Check if files exist
    if (!fs::exists(vsPath)) {
        LOG_ERROR("Vertex shader file not found: " + vsPath);
        return false;
    }
    
    if (!fs::exists(fsPath)) {
        LOG_ERROR("Fragment shader file not found: " + fsPath);
        return false;
    }
    
    // Load shader
    outShader = ::LoadShader(vsPath.c_str(), fsPath.c_str());
    
    if (outShader.id == 0) {
        LOG_ERROR("Failed to compile shader from " + vsPath + " and " + fsPath);
        return false;
    }
    
    return true;
}

void ShaderSystem::SetupDefaultUniforms(ShaderData& shaderData) {
    // Cache common uniform locations for performance
    Shader& shader = shaderData.shader;
    
    // Standard model/view/projection matrices
    shader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(shader, "mvp");
    shader.locs[SHADER_LOC_MATRIX_VIEW] = GetShaderLocation(shader, "matView");
    shader.locs[SHADER_LOC_MATRIX_PROJECTION] = GetShaderLocation(shader, "matProjection");
    shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader, "matModel");
    
    // Lighting
    shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader, "viewPos");
    
    // Material properties
    shaderData.uniformLocations["texture0"] = GetShaderLocation(shader, "texture0");
    shaderData.uniformLocations["colDiffuse"] = GetShaderLocation(shader, "colDiffuse");
    
    LOG_DEBUG("Set up default uniforms for shader ID " + std::to_string(shaderData.shader.id));
}

int ShaderSystem::GetUniformLocation(uint32_t shaderId, const std::string& uniformName) {
    auto it = shaders_.find(shaderId);
    if (it == shaders_.end() || !it->second) {
        return -1;
    }
    
    auto& uniformMap = it->second->uniformLocations;
    auto uniformIt = uniformMap.find(uniformName);
    
    if (uniformIt != uniformMap.end()) {
        return uniformIt->second;
    }
    
    // Cache the location for future use
    int location = GetShaderLocation(it->second->shader, uniformName.c_str());
    uniformMap[uniformName] = location;
    
    return location;
}

void ShaderSystem::SetShaderUniforms(uint32_t shaderId, const std::unordered_map<std::string, float>& uniforms) {
    for (const auto& uniform : uniforms) {
        int location = GetUniformLocation(shaderId, uniform.first);
        if (location >= 0) {
            Shader* shader = GetShader(shaderId);
            if (shader) {
                SetShaderValue(*shader, location, &uniform.second, SHADER_UNIFORM_FLOAT);
            }
        }
    }
}

void ShaderSystem::LogShaderStatus() const {
    LOG_INFO("ShaderSystem Status:");
    LOG_INFO("  Total Shaders: " + std::to_string(shaders_.size()));
    LOG_INFO("  Basic Shader ID: " + std::to_string(basicShaderId_));
    LOG_INFO("  PBR Shader ID: " + std::to_string(pbrShaderId_));
    LOG_INFO("  Shader Directory: " + shaderDirectory_);
}
