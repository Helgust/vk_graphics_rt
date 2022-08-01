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

#define EPSILON 0.15
#define SHADOW_OPACITY 0.5

void main() 
{
    vec2 uv = gl_FragCoord.xy / params.screenSize;
    vec3 fragPos = texture(samplerPosition, uv).rgb;
	vec3 normal = texture(samplerNormal, uv).rgb;
	vec4 albedo = texture(samplerAlbedo, uv);
    

    
    // gl_FragDepth = gl_FragCoord.z * params.isOutsideLight;

    // vec4 coord = params.mProjView * vec4(fragPos, 1.0f);

    // vec4 albedo = vec4(0.9f, 0.92f, 1.0f, 1.0f);

    vec3 lightDir1 = normalize(UboParams.lights[0].pos.xyz -fragPos);
    vec3 lightVec = fragPos - UboParams.lights[0].pos.xyz;
    float lightDist = length(lightVec);
    float lightRadius = UboParams.lights[0].radius;
    float sampledDist = texture(shadowCubeMap, lightVec).r;
    float shadow = (lightDist <= sampledDist + EPSILON) ? 1.0 : SHADOW_OPACITY;
    // debugPrintfEXT("shadow = %1.2f\n", shadow);

    if (lightDist > lightRadius) {
        outFragcolor = vec4(0.0f);
        return;
    }
    // vec3 lightDir2 = vec3(0.0f, 0.0f, 1.0f);

    const vec4 dark_violet = vec4(0.59f, 0.0f, 0.82f, 1.0f);
    const vec4 chartreuse  = vec4(0.5f, 1.0f, 0.0f, 1.0f);

    vec4 lightColor1 = UboParams.lights[0].color;
    // if(Params.animateLightColor)
    //     lightColor1 = mix(dark_violet, chartreuse, abs(sin(Params.time)));

    // vec4 lightColor2 = vec4(1.0f, 1.0f, 1.0f, 1.0f);

    vec3 N = normal; 

    vec4 color1 = max(dot(N, lightDir1), 0.0f) * lightColor1;
    // vec4 color2 = max(dot(N, lightDir2), 0.0f) * lightColor2;
    // vec4 color_lights = mix(color1, color2, 0.2f);
    float intensity = 1.f;
    float attn = clamp(1.0 - lightDist*lightDist/(lightRadius*lightRadius), 0.0, 1.0); 
    attn *= attn;
    outFragcolor = color1 * albedo * shadow;    
    //outFragcolor= vec4(1.0, 0.0f, 0.0f, 1.0f);
}