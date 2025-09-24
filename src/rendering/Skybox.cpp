#include "Skybox.h"
#include "raylib.h"
#include "rlgl.h"
#include "../utils/Logger.h"
#include "../utils/PathUtils.h"

Skybox::Skybox() : cubemap_{0}, shader_{0}, model_{0}, loaded_(false) {}

Skybox::~Skybox() {
    Unload();
}

bool Skybox::LoadFromFile(const std::string& filePath) {
    Unload();
    std::string exeDir = Utils::GetExecutableDir();
    // Always use executable-relative path for cubemap, assets are copied to build/bin/assets/
    std::string cubemapPath = exeDir + "/assets/" + filePath;
    LOG_INFO("SKYBOX: Attempting to load cubemap image from: " + cubemapPath);
    Image img = LoadImage(cubemapPath.c_str());
    if (img.data == nullptr || img.width == 0 || img.height == 0) {
        LOG_ERROR("SKYBOX: Failed to load image: " + cubemapPath);
        // Try fallback paths, all relative to exeDir/assets
        std::string fallback1 = exeDir + "/assets/textures/skybox.png";
        LOG_INFO("SKYBOX: Attempting to load cubemap image from: " + fallback1);
        img = LoadImage(fallback1.c_str());
        if (img.data == nullptr) {
            LOG_ERROR("SKYBOX: Failed to load image: " + fallback1);
            std::string fallback2 = exeDir + "/assets/textures/cubemap.png";
            LOG_INFO("SKYBOX: Attempting to load cubemap image from: " + fallback2);
            img = LoadImage(fallback2.c_str());
            if (img.data == nullptr) {
                LOG_ERROR("SKYBOX: Failed to load image: " + fallback2);
                std::string fallback3 = exeDir + "/assets/skybox/cloudy.png";
                LOG_INFO("SKYBOX: Attempting to load cubemap image from: " + fallback3);
                img = LoadImage(fallback3.c_str());
                if (img.data == nullptr) {
                    LOG_ERROR("SKYBOX: Failed to load image: " + fallback3);
                    return false;
                }
            }
        }
    }
    cubemap_ = LoadTextureCubemap(img, CUBEMAP_LAYOUT_AUTO_DETECT);
    if (cubemap_.id == 0) {
        cubemap_ = LoadTextureCubemap(img, CUBEMAP_LAYOUT_CROSS_FOUR_BY_THREE);
    }
    if (cubemap_.id == 0) {
        cubemap_ = LoadTextureCubemap(img, CUBEMAP_LAYOUT_CROSS_THREE_BY_FOUR);
    }

    // Only unload image after successful cubemap creation
    if (cubemap_.id != 0) {
        UnloadImage(img);
        LOG_INFO("SKYBOX: Successfully created cubemap from image");
    } else {
        UnloadImage(img);
        LOG_ERROR("SKYBOX: Failed to create cubemap from image: " + cubemapPath);
        LOG_ERROR("SKYBOX: Falling back to procedural test skybox.");
        if (!LoadTestSkybox()) {
            LOG_ERROR("SKYBOX: Fallback test skybox also failed. Skybox will not render.");
            loaded_ = false;
            return false;
        }
        return true;
    }
    // Generate a cube mesh exactly like the raylib example
    Mesh cube = GenMeshCube(1.0f, 1.0f, 1.0f);
    model_ = LoadModelFromMesh(cube);
    model_.materials[0].maps[MATERIAL_MAP_CUBEMAP].texture = cubemap_;

    // Improve texture filtering for better quality
    SetTextureFilter(cubemap_, TEXTURE_FILTER_TRILINEAR);
    SetTextureWrap(cubemap_, TEXTURE_WRAP_CLAMP);

    std::string vsPath = exeDir + "/shaders/skybox/skybox.vs";
    std::string fsPath = exeDir + "/shaders/skybox/skybox.fs";
    LOG_INFO("SKYBOX: Attempting to load vertex shader from: " + vsPath);
    LOG_INFO("SKYBOX: Attempting to load fragment shader from: " + fsPath);
    shader_ = LoadShader(vsPath.c_str(), fsPath.c_str());
    LOG_INFO("SKYBOX: Shader loaded with ID: " + std::to_string(shader_.id));

    if (shader_.id == 0) {
        LOG_ERROR("SKYBOX: Failed to load/compile shader!");
        return false;
    }

    model_.materials[0].shader = shader_;

    // Set up shader uniforms exactly like the raylib example
    int envMap = MATERIAL_MAP_CUBEMAP;
    int zero = 0;
    SetShaderValue(shader_, GetShaderLocation(shader_, "environmentMap"), &envMap, SHADER_UNIFORM_INT);
    SetShaderValue(shader_, GetShaderLocation(shader_, "doGamma"), &zero, SHADER_UNIFORM_INT);  // Not using HDR
    SetShaderValue(shader_, GetShaderLocation(shader_, "vflipped"), &zero, SHADER_UNIFORM_INT); // Not using HDR

    LOG_INFO("SKYBOX: Shader uniforms set up like raylib example");
    loaded_ = true;
    LOG_INFO("SKYBOX: Successfully loaded skybox from file: " + cubemapPath);
    LOG_INFO("SKYBOX: Cubemap ID: " + std::to_string(cubemap_.id) +
             ", Shader ID: " + std::to_string(shader_.id) +
             ", Model meshCount: " + std::to_string(model_.meshCount));
    return true;
}

bool Skybox::LoadTestSkybox() {
    Unload();
    const int size = 512;
    Image img = GenImageGradientRadial(size, size, 0.0f, SKYBLUE, DARKBLUE);
    cubemap_ = LoadTextureCubemap(img, CUBEMAP_LAYOUT_AUTO_DETECT);
    UnloadImage(img);
    if (cubemap_.id == 0) {
        LOG_ERROR("SKYBOX: Failed to create test skybox");
        return false;
    }
    Mesh cube = GenMeshCube(2.0f, 2.0f, 2.0f);
    model_ = LoadModelFromMesh(cube);
    model_.materials[0].maps[MATERIAL_MAP_CUBEMAP].texture = cubemap_;
    shader_ = LoadShader(0, 0);
    model_.materials[0].shader = shader_;
    loaded_ = true;
    LOG_INFO("SKYBOX: Test skybox initialized");
    return true;
}

void Skybox::Unload() {
    UnloadResources();
    loaded_ = false;
}

void Skybox::UnloadResources() {
    if (cubemap_.id != 0) {
        UnloadTexture(cubemap_);
        cubemap_ = (TextureCubemap){0};
    }
    if (shader_.id != 0) {
        UnloadShader(shader_);
        shader_ = (Shader){0};
    }
    if (model_.meshes != nullptr) {
        UnloadModel(model_);
        model_ = (Model){0};
    }
}

void Skybox::Render(const Camera3D& camera) {
    if (!loaded_ || cubemap_.id == 0) {
        static int notLoadedCount = 0;
        notLoadedCount++;
        if (notLoadedCount % 60 == 0) {
            LOG_DEBUG("SKYBOX: Render called but skybox not loaded or cubemap invalid");
        }
        return;
    }

    static int renderCount = 0;
    renderCount++;

    if (renderCount % 60 == 0) {
        LOG_DEBUG("SKYBOX: Starting skybox render - Camera pos: (" +
                  std::to_string(camera.position.x) + ", " +
                  std::to_string(camera.position.y) + ", " +
                  std::to_string(camera.position.z) + ")");
    }

    // Skybox rendering - exactly like the raylib official example
    rlDisableBackfaceCulling();
    rlDisableDepthMask();
    rlDisableDepthTest();

    // Draw at origin with scale 1.0 - shader handles infinite distance effect
    // The shader's gl_Position = pos.xyww ensures it's always at far plane
    Vector3 origin = {0.0f, 0.0f, 0.0f};
    DrawModel(model_, origin, 1.0f, WHITE);

    rlEnableBackfaceCulling();
    rlEnableDepthMask();
    rlEnableDepthTest();

    if (renderCount % 60 == 0) {
        LOG_DEBUG("SKYBOX: Rendered at origin with scale 1.0 (shader handles infinite distance)");
    }
}

bool Skybox::IsLoaded() const {
    return loaded_;
}
