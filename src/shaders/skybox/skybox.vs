#version 330 core

// Skybox Vertex Shader - removes camera translation so the skybox stays centered

// Input
in vec3 vertexPosition;

// Output
out vec3 fragPosition;

// Raylib-provided matrices
uniform mat4 matProjection;
uniform mat4 matView;
uniform mat4 matModel;

void main()
{
    // Use local vertex position as direction for cubemap sampling
    fragPosition = vertexPosition;

    // Remove translation from the view matrix to keep the cube centered on the camera
    mat4 viewNoTranslation = matView;
    viewNoTranslation[3][0] = 0.0;
    viewNoTranslation[3][1] = 0.0;
    viewNoTranslation[3][2] = 0.0;

    vec4 pos = matProjection * viewNoTranslation * matModel * vec4(vertexPosition, 1.0);

    // Force depth to far plane so it renders behind everything
    gl_Position = vec4(pos.xy, pos.w, pos.w);
}
