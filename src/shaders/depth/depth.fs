#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;

// Output fragment color
out vec4 finalColor;

void main()
{
    // Depth shader - output white color
    // Depth is automatically written to the depth buffer by OpenGL
    finalColor = vec4(1.0, 1.0, 1.0, 1.0);
}
