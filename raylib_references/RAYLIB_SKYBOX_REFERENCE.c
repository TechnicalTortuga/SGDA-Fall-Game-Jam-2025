/*
* RAYLIB OFFICIAL SKYBOX REFERENCE IMPLEMENTATION
* Source: https://github.com/raysan5/raylib/blob/master/examples/models/models_skybox.c
*
* This is the official raylib skybox example showing the correct way to:
* 1. Load and setup skybox shaders
* 2. Generate cubemap from HDR panorama texture
* 3. Render skybox properly in 3D scene
*
* Key Points for Implementation:
* - Uses specific skybox shaders (skybox.vs/skybox.fs)
* - Generates cubemap from HDR panorama using GenTextureCubemap()
* - Sets environmentMap uniform for shader
* - Renders skybox as a simple cube model
*/

#include "raylib.h"

int main(void)
{
    // Initialization
    const int screenWidth = 800;
    const int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [models] example - skybox loading and drawing");

    // Define the camera to look into our 3d world
    Camera camera = { { 1.0f, 1.0f, 1.0f }, { 4.0f, 1.0f, 4.0f }, { 0.0f, 1.0f, 0.0f }, 45.0f, 0 };

    // Load skybox model
    Mesh cube = GenMeshCube(1.0f, 1.0f, 1.0f);
    Model skybox = LoadModelFromMesh(cube);

    // CRITICAL: Load skybox shader and set required locations
    #if defined(PLATFORM_DESKTOP)
    skybox.materials[0].shader = LoadShader("resources/shaders/glsl330/skybox.vs", "resources/shaders/glsl330/skybox.fs");
    #else
    skybox.materials[0].shader = LoadShader("resources/shaders/glsl100/skybox.vs", "resources/shaders/glsl100/skybox.fs");
    #endif

    // CRITICAL: Set environmentMap uniform for the shader
    SetShaderValue(skybox.materials[0].shader, GetShaderLocation(skybox.materials[0].shader, "environmentMap"), (int[1]){ MAP_CUBEMAP }, UNIFORM_INT);

    // Load cubemap generation shader
    #if defined(PLATFORM_DESKTOP)
    Shader shdrCubemap = LoadShader("resources/shaders/glsl330/cubemap.vs", "resources/shaders/glsl330/cubemap.fs");
    #else
    Shader shdrCubemap = LoadShader("resources/shaders/glsl100/cubemap.vs", "resources/shaders/glsl100/cubemap.fs");
    #endif

    // APPROACH 1: Generate cubemap from HDR panorama texture
    Texture2D texHDR = LoadTexture("resources/dresden_square.hdr");
    skybox.materials[0].maps[MAP_CUBEMAP].texture = GenTextureCubemap(shdrCubemap, texHDR, 512);
    UnloadTexture(texHDR); // Texture not required anymore, cubemap already generated
    UnloadShader(shdrCubemap); // Unload cubemap generation shader, not required anymore

    // APPROACH 2: Load cubemap directly (alternative to HDR generation)
    // skybox.materials[0].maps[MAP_CUBEMAP].texture = LoadTextureCubemap(image, CUBEMAP_LAYOUT_AUTO_DETECT);

    SetCameraMode(camera, CAMERA_FIRST_PERSON); // Set a first person camera mode
    SetTargetFPS(60); // Set our game to run at 60 frames-per-second

    // Main game loop
    while (!WindowShouldClose())
    {
        UpdateCamera(&camera); // Update camera

        BeginDrawing();
        ClearBackground(RAYWHITE);

        BeginMode3D(camera);

        // CRITICAL: Render skybox FIRST (background)
        rlDisableBackfaceCulling();     // Disable backface culling for skybox
        rlDisableDepthMask();           // Disable depth writing for skybox
        rlDisableDepthTest();           // Disable depth testing for skybox

        DrawModel(skybox, (Vector3){0, 0, 0}, 1.0f, WHITE);

        rlEnableBackfaceCulling();      // Re-enable backface culling
        rlEnableDepthMask();            // Re-enable depth writing
        rlEnableDepthTest();            // Re-enable depth testing

        // Draw other 3D objects AFTER skybox
        DrawGrid(10, 1.0f);

        EndMode3D();

        DrawFPS(10, 10);
        EndDrawing();
    }

    // De-Initialization
    UnloadShader(skybox.materials[0].shader);
    UnloadTexture(skybox.materials[0].maps[MAP_CUBEMAP].texture);
    UnloadModel(skybox); // Unload skybox model
    CloseWindow(); // Close window and OpenGL context

    return 0;
}

/*
* SHADER FILES NEEDED:
*
* skybox.vs (Vertex Shader):
* - Transforms skybox vertices
* - Passes position as texture coordinates for cubemap sampling
*
* skybox.fs (Fragment Shader):
* - Samples from cubemap texture using 3D coordinates
* - Uses textureCube() to sample the appropriate face
*
* cubemap.vs/cubemap.fs (For HDR generation):
* - Converts equirectangular HDR to cubemap faces
* - Only needed if generating from panorama textures
*/
