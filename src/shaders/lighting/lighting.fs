#version 330

// Input vertex attributes (from vertex shader)
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Output fragment color
out vec4 finalColor;

// NOTE: Add your custom variables here

#define     MAX_LIGHTS              16
#define     LIGHT_DIRECTIONAL       0
#define     LIGHT_POINT             1

struct Light {
    int enabled;
    int type;
    vec3 position;
    vec3 target;
    vec4 color;
    float attenuation;
};

// Input lighting values
uniform Light lights[MAX_LIGHTS];
uniform vec4 ambient;
uniform vec3 viewPos;
uniform int activeLightCount;

void main()
{
    // Texel color fetching from texture sampler
    vec4 texelColor = texture(texture0, fragTexCoord);
    vec3 lightDot = vec3(0.0);
    vec3 normal = normalize(fragNormal);
    vec3 viewD = normalize(viewPos - fragPosition);
    vec3 specular = vec3(0.0);

    vec4 tint = colDiffuse*fragColor;

    // NOTE: Implement here your fragment shader code

    for (int i = 0; i < min(activeLightCount, MAX_LIGHTS); i++)  // Only process active lights
    {
        if (lights[i].enabled == 1)
        {
            vec3 light = vec3(0.0);

            if (lights[i].type == LIGHT_DIRECTIONAL)
            {
                light = -normalize(lights[i].target - lights[i].position);
            }

            if (lights[i].type == LIGHT_POINT)
            {
                light = normalize(lights[i].position - fragPosition);
            }

            float NdotL = max(dot(normal, light), 0.0);

            // Calculate distance attenuation for point lights
            float attenuation = 1.0;
            if (lights[i].type == LIGHT_POINT) {
                float distance = length(lights[i].position - fragPosition);
                attenuation = 1.0 / (1.0 + lights[i].attenuation * distance * distance);
            }

            // Apply distance attenuation and add diffuse lighting (reduced intensity)
            lightDot += lights[i].color.rgb * NdotL * attenuation * 0.6; // Reduced from full to 60%

            // Very subtle specular reflection - almost matte surfaces
            float specCo = 0.0;
            if (NdotL > 0.0) {
                specCo = pow(max(0.0, dot(viewD, reflect(-(light), normal))), 2.0) * 0.05; // Much more subtle
            }
            specular += specCo * attenuation;
        }
    }

    // Improve blending with smoother tone mapping
    vec3 lighting = lightDot + specular;

    // More aggressive tone mapping for smoother transitions
    lighting = lighting / (lighting + vec3(0.5));

    // Combine diffuse and lighting with reduced contrast
    finalColor = texelColor * tint * vec4(lighting * 0.8 + vec3(0.2), 1.0); // Add base illumination

    // Reduced ambient lighting for more pronounced light effects
    finalColor += texelColor * (ambient/10.0) * tint;

    // Lower ambient floor to allow more light variation
    finalColor = max(finalColor, texelColor * (ambient/15.0) * tint);

    // Gamma correction
    finalColor = pow(finalColor, vec4(1.0/2.2));
}
