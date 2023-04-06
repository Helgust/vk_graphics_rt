#version 450
#extension GL_GOOGLE_include_directive : require
//#extension GL_EXT_debug_printf : enable#extension GL_EXT_debug_printf : enable

#include "common.h"

layout(binding = 0, set = 0) uniform AppData
{
    UniformParams UboParams;
};

layout (binding = 1) uniform sampler2D samplerPosition;
layout (binding = 2) uniform sampler2D samplerNormal;
layout (binding = 3) uniform sampler2D samplerAlbedo;
layout (binding = 4) uniform sampler2D samplerDepth;
//layout (binding = 5) uniform samplerCube shadowCubeMap;
layout (binding = 6) uniform sampler2D samplerVelocity;
layout (binding = 7) uniform sampler2D samplerSoftRtImage;
layout (binding = 8) uniform sampler2D samplerRtImageDynamic;
layout (binding = 9) uniform sampler2D samplerRtImageStatic;

layout(push_constant) uniform params_t
{
    mat4 mProjView;
    mat4 mModel;
    mat4 lightMatrix;
    vec4 color;
    vec4 vehiclePos;
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
    float depth = texture(samplerDepth, uv).x;

    float softShadow = texture(samplerSoftRtImage, uv).x;

    vec4 lightColor1 = UboParams.lights[0].color;
    vec3 N = normal; 

    vec4 color1 = max(dot(N, normalize(UboParams.lights[0].dir.xyz)), 0.0f) * lightColor1;
    // vec4 color2 = max(dot(N, lightDir2), 0.0f) * lightColor2;
    // vec4 color_lights = mix(color1, color2, 0.2f);
    float intensity = 1.f;
    // float attn = clamp(1.0 - lightDist*lightDist/(lightRadius*lightRadius), 0.0, 1.0); 
    // attn *= attn;
    outFragcolor = color1 * albedo;
    switch (int(UboParams.m_time_gbuffer_index.w)) {
    case 0:
        if(UboParams.settings.y == 1)
            outFragcolor *= softShadow;
        break;
    case 1:
        outFragcolor = vec4(fragPos.xyz,1.0f);
        // mat4 mInvProjView = inverse(params.mProjView); 
        // vec4 screenSpacePos = vec4( 2.0f * uv - 1.0f, texture(samplerDepth, uv).x, 1.0f);
        // vec4 camSpacePos  = mInvProjView * screenSpacePos;
        // vec3 position = camSpacePos.xyz / camSpacePos.w;
        // outFragcolor = vec4(position, 1.0);
        break;
    case 2:
        outFragcolor = vec4(normal.xyz,1.0f);
        break;
    case 3:
        outFragcolor = albedo;
        break;
    case 4:
        outFragcolor = vec4(depth, 0.0f, 0.0f, 1.0f);
        break;
    case 5:
        outFragcolor = vec4(texture(samplerVelocity, uv).xy + 0.5f, 0.0f, 1.0f);
        break;
    case 6:
        outFragcolor = vec4(texture(samplerRtImageStatic, uv).x);
        break;
    case 7:
        outFragcolor = vec4(texture(samplerRtImageStatic, uv));
        break;
    case 8:
        outFragcolor = vec4(texture(samplerRtImageDynamic, uv).x);
        break;
    case 9:
        outFragcolor = vec4(texture(samplerRtImageDynamic, uv));
        break;
    case 10:
        outFragcolor = vec4(texture(samplerSoftRtImage, uv));
        break;
    default:
        outFragcolor = vec4(0.0f, 1.0f, 0.0f, 1.0f);
    }
}