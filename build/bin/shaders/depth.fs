#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;

// Output fragment color
out vec4 finalColor;

void main()
{
    // Depth shader - just output depth
    // The depth is automatically written to the depth buffer
    finalColor = vec4(1.0, 1.0, 1.0, 1.0);
}
