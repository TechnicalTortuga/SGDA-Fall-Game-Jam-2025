#version 330 core

// Skybox Fragment Shader
// Debug version to test cubemap sampling

// Input from vertex shader
in vec3 fragPosition;       // position from vertex shader (used as direction vector)

// Output
out vec4 finalColor;        // final fragment color

// Uniforms
uniform samplerCube environmentMap;    // cubemap texture sampler
uniform int doGamma;                   // gamma correction flag
uniform int vflipped;                  // vertical flip flag

void main()
{
    // Sample the cubemap using the direction vector from the vertex shader
    vec3 color = texture(environmentMap, fragPosition).rgb;
    finalColor = vec4(color, 1.0);
}
