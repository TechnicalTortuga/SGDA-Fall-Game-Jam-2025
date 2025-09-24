#version 330

// Fragment shader for basic textured rendering with simple lighting
// Input from vertex shader
in vec2 fragTexCoord;      // Texture coordinates
in vec3 fragNormal;        // Normal in world space
in vec3 fragPosition;      // Position in world space

// Uniforms
uniform sampler2D texture0; // Diffuse texture
uniform vec4 colDiffuse;    // Diffuse color tint
uniform vec3 viewPos;       // Camera position for lighting

// Output
out vec4 finalColor;

void main()
{
    // Sample the diffuse texture
    vec4 texelColor = texture(texture0, fragTexCoord);
    
    // Apply color tint
    vec4 baseColor = texelColor * colDiffuse;
    
    // Simple directional lighting
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.5)); // Light direction
    vec3 normal = normalize(fragNormal);
    
    // Calculate diffuse lighting
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Ambient lighting
    float ambient = 0.3;
    
    // Combine lighting
    float lighting = ambient + diff * 0.7;
    
    // Apply lighting to base color
    finalColor = vec4(baseColor.rgb * lighting, baseColor.a);
    
    // Ensure minimum visibility (prevent completely black surfaces)
    if (finalColor.a > 0.0) {
        finalColor.rgb = max(finalColor.rgb, vec3(0.1) * baseColor.rgb);
    }
}
