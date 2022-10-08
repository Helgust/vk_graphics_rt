#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_debug_printf : enable

#include "common.h"

layout(binding = 0, set = 0) uniform AppData
{
    UniformParams UboParams;
};

layout (binding = 1) uniform sampler2D samplerPosition;
layout (binding = 2) uniform sampler2D samplerNormal;
layout (binding = 3) uniform sampler2D samplerAlbedo;
layout (binding = 4) uniform sampler2D samplerDepth;
layout (binding = 5) uniform samplerCube shadowCubeMap;
layout (binding = 6) uniform sampler2D samplerVelocity;
layout (binding = 7) uniform sampler2D samplerRtImage;

layout(push_constant) uniform params_t
{
    mat4 mProjView;
    mat4 mModel;
    mat4 lightMatrix;
    vec4 color;
    vec2 screenSize; 
} params;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragcolor;

#define EPSILON 0.01
#define SHADOW_OPACITY 0.0

void main() 
{
    vec2 uv = gl_FragCoord.xy / params.screenSize;
    vec3 fragPos = texture(samplerPosition, uv).rgb;
	vec3 normal = texture(samplerNormal, uv).rgb;
	vec4 albedo = texture(samplerAlbedo, uv);

    vec3 lightDir1 = normalize(UboParams.lights[0].pos.xyz -fragPos);
    vec3 lightVec = fragPos - UboParams.lights[0].pos.xyz;
    float lightDist = length(lightVec);
    float lightRadius = UboParams.lights[0].radius_lightDist_dummies.y;
    float sampledDist = texture(shadowCubeMap, lightVec).r;
    float shadow = (lightDist <= sampledDist + EPSILON) ? 1.0 : SHADOW_OPACITY;
    float softShadow = texture(samplerRtImage, uv).x;

    vec4 lightColor1 = UboParams.lights[0].color;
    vec3 N = normal; 

    vec4 color1 = max(dot(N, lightDir1), 0.0f) * lightColor1;
    // vec4 color2 = max(dot(N, lightDir2), 0.0f) * lightColor2;
    // vec4 color_lights = mix(color1, color2, 0.2f);
    float intensity = 1.f;
    float attn = clamp(1.0 - lightDist*lightDist/(lightRadius*lightRadius), 0.0, 1.0); 
    attn *= attn;
    outFragcolor = color1 * albedo * attn;
    switch (int(UboParams.m_time_gbuffer_index.w)) {
    case 0:
        if(UboParams.settings.y == 1)
            outFragcolor *= softShadow;
        else
            outFragcolor *= shadow;
        break;
    case 1:
        outFragcolor = vec4(fragPos.xyz,1.0f);
        break;
    case 2:
        outFragcolor = vec4(normal.xyz,1.0f);
        break;
    case 3:
        outFragcolor = albedo;
        break;
    case 4:
        outFragcolor = vec4(sampledDist/255.0f, 0.0f, 0.0f, 1.0f);
        break;
    case 5:
        outFragcolor = texture(samplerVelocity, uv);
        break;
    case 6:
        outFragcolor = texture(samplerRtImage, uv);
        break;
    }
}