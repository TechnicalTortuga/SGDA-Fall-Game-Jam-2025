#version 330

// Input vertex attributes (from vertex shader)
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Shadow mapping uniforms
uniform mat4 lightVP;
uniform sampler2D shadowMap;
uniform int shadowMapResolution;
uniform int shadowsEnabled;

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

// Shadow mapping (for first directional light only)
uniform mat4 lightVP; // Light source view-projection matrix
uniform sampler2D shadowMap;
uniform int shadowMapResolution;
uniform int shadowsEnabled;

void main()
{
    // Texel color fetching from texture sampler
    vec4 texelColor = texture(texture0, fragTexCoord);
    vec3 lightDot = vec3(0.0);
    vec3 normal = normalize(fragNormal);
    vec3 viewD = normalize(viewPos - fragPosition);
    vec3 specular = vec3(0.0);

    vec4 tint = colDiffuse*fragColor;

    // Shadow calculations (Raylib approach)
    float shadowFactor = 0.0; // 0.0 = no shadow, 1.0 = full shadow

    // Debug: Force some shadow effect to test pipeline
    // shadowFactor = 0.3; // Uncomment to test if shadows work at all

    // Calculate shadows for first directional light only
    if (shadowsEnabled == 1 && activeLightCount > 0 && lights[0].enabled == 1 && lights[0].type == LIGHT_DIRECTIONAL) {
        vec4 fragPosLightSpace = lightVP * vec4(fragPosition, 1.0);
        fragPosLightSpace.xyz /= fragPosLightSpace.w; // Perspective division
        fragPosLightSpace.xyz = (fragPosLightSpace.xyz + 1.0) / 2.0; // Transform to [0, 1] range

        vec2 sampleCoords = fragPosLightSpace.xy;
        // Flip Y coordinate for OpenGL texture coordinates
        sampleCoords.y = 1.0 - sampleCoords.y;
        float curDepth = fragPosLightSpace.z;

        // Only calculate shadows if we're within the shadow map bounds
        if (sampleCoords.x >= 0.0 && sampleCoords.x <= 1.0 &&
            sampleCoords.y >= 0.0 && sampleCoords.y <= 1.0 &&
            curDepth <= 1.0) {

            vec3 lightDir = -normalize(lights[0].target - lights[0].position);
            float bias = max(0.0002 * (1.0 - dot(normal, lightDir)), 0.00002) + 0.00001;

            int shadowCounter = 0;
            const int numSamples = 9;

            // PCF (percentage-closer filtering) for soft shadows
            vec2 texelSize = vec2(1.0 / float(shadowMapResolution));
            for (int x = -1; x <= 1; x++) {
                for (int y = -1; y <= 1; y++) {
                    float sampleDepth = texture(shadowMap, sampleCoords + texelSize * vec2(x, y)).r;
                    if (curDepth - bias > sampleDepth) shadowCounter++;
                }
            }
            shadowFactor = float(shadowCounter) / float(numSamples); // 0.0 to 1.0 range
        }
    }

    // Process all lights
    for (int i = 0; i < min(activeLightCount, MAX_LIGHTS); i++) {
        if (lights[i].enabled == 1) {
            vec3 light = vec3(0.0);

            if (lights[i].type == LIGHT_DIRECTIONAL) {
                light = -normalize(lights[i].target - lights[i].position);
            }

            if (lights[i].type == LIGHT_POINT) {
                light = normalize(lights[i].position - fragPosition);
            }

            float NdotL = max(dot(normal, light), 0.0);

            // Calculate distance attenuation for point lights
            float attenuation = 1.0;
            if (lights[i].type == LIGHT_POINT) {
                float distance = length(lights[i].position - fragPosition);
                attenuation = 1.0 / (1.0 + lights[i].attenuation * distance * distance);
            }

            // Apply distance attenuation (increased intensity)
            lightDot += lights[i].color.rgb * NdotL * attenuation * 1.5;

            // Specular reflection
            float specCo = 0.0;
            if (NdotL > 0.0) {
                specCo = pow(max(0.0, dot(viewD, reflect(-(light), normal))), 4.0) * 0.2;
            }
            specular += specCo * attenuation;
        }
    }

    // Better blending: avoid oversaturation with tone mapping
    vec3 lighting = lightDot + specular;

    // Simple tone mapping to prevent oversaturation
    lighting = lighting / (lighting + vec3(1.0));

    // Combine diffuse and lighting
    finalColor = texelColor * tint * vec4(lighting, 1.0);

    // Apply shadows globally (Raylib approach)
    finalColor = mix(finalColor, vec4(0.0, 0.0, 0.0, 1.0), shadowFactor);

    // Add reduced ambient lighting to let light colors show through
    finalColor += texelColor * (ambient/12.0) * tint;

    // Ensure we don't go below ambient
    finalColor = max(finalColor, texelColor * (ambient/20.0) * tint);

    // Gamma correction
    finalColor = pow(finalColor, vec4(1.0/2.2));
}
