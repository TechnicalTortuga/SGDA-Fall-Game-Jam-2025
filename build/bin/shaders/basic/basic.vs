#version 330

// Vertex shader for basic textured rendering
// Input vertex attributes
in vec3 vertexPosition;    // Vertex position in model space
in vec2 vertexTexCoord;    // Vertex texture coordinates
in vec3 vertexNormal;      // Vertex normal

// Input uniforms
uniform mat4 mvp;          // Model-View-Projection matrix
uniform mat4 matModel;     // Model transformation matrix
uniform mat4 matView;      // View matrix  
uniform mat4 matProjection; // Projection matrix

// Output to fragment shader
out vec2 fragTexCoord;     // Texture coordinates
out vec3 fragNormal;       // Normal in world space
out vec3 fragPosition;     // Position in world space

void main()
{
    // Pass texture coordinates to fragment shader with V-flip for correct orientation
    fragTexCoord = vec2(vertexTexCoord.x, 1.0 - vertexTexCoord.y);
    
    // Transform normal to world space
    fragNormal = normalize(vec3(matModel * vec4(vertexNormal, 0.0)));
    
    // Transform position to world space
    fragPosition = vec3(matModel * vec4(vertexPosition, 1.0));
    
    // Calculate final vertex position
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
