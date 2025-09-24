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
};

// Input lighting values
uniform Light lights[MAX_LIGHTS];
uniform vec4 ambient;
uniform vec3 viewPos;
uniform int activeLightCount;

// Shadow mapping (for first directional light)
uniform mat4 lightVP;
uniform sampler2D shadowMap;
uniform bool shadowsEnabled;

void main()
{
    // Texel color fetching from texture sampler
    vec4 texelColor = texture(texture0, fragTexCoord);
    vec3 lightDot = vec3(0.0);
    vec3 normal = normalize(fragNormal);
    vec3 viewD = normalize(viewPos - fragPosition);
    vec3 specular = vec3(0.0);

    vec4 tint = colDiffuse*fragColor;

    // Shadow mapping function
    float ShadowCalculation(vec4 fragPosLightSpace)
    {
        // Perform perspective divide
        vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

        // Transform to [0,1] range
        projCoords = projCoords * 0.5 + 0.5;

        // Get closest depth value from light's perspective
        float closestDepth = texture(shadowMap, projCoords.xy).r;

        // Get depth of current fragment from light's perspective
        float currentDepth = projCoords.z;

        // Check whether current frag pos is in shadow
        float shadow = 0.0;
        vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
        for(int x = -1; x <= 1; ++x)
        {
            for(int y = -1; y <= 1; ++y)
            {
                float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
                shadow += currentDepth - 0.005 > pcfDepth ? 1.0 : 0.0;
            }
        }
        shadow /= 9.0;

        // Keep the shadow at 0.0 when outside the far_plane region of the light's frustum.
        if(projCoords.z > 1.0)
            shadow = 0.0;

        return shadow;
    }

    // Calculate shadows for first directional light
    float shadow = 0.0;
    if (shadowsEnabled && activeLightCount > 0 && lights[0].enabled == 1 && lights[0].type == LIGHT_DIRECTIONAL)
    {
        vec4 fragPosLightSpace = lightVP * vec4(fragPosition, 1.0);
        shadow = ShadowCalculation(fragPosLightSpace);
    }

    // NOTE: Implement here your fragment shader code

    for (int i = 0; i < min(activeLightCount, MAX_LIGHTS); i++)
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

            // Apply shadows (only for directional light, first one)
            float shadowFactor = (i == 0 && lights[i].type == LIGHT_DIRECTIONAL) ? (1.0 - shadow) : 1.0;

            lightDot += lights[i].color.rgb*NdotL * shadowFactor;

            float specCo = 0.0;
            if (NdotL > 0.0) specCo = pow(max(0.0, dot(viewD, reflect(-(light), normal))), 16.0) * shadowFactor; // 16 refers to shine
            specular += specCo;
        }
    }

    finalColor = (texelColor*((tint + vec4(specular, 1.0))*vec4(lightDot, 1.0)));

    // Stronger ambient lighting for better base illumination
    finalColor += texelColor*(ambient/3.0)*tint;

    // Ensure minimum illumination level to prevent completely dark faces
    finalColor = max(finalColor, texelColor*(ambient/5.0)*tint);

    // Gamma correction
    finalColor = pow(finalColor, vec4(1.0/2.2));
}
