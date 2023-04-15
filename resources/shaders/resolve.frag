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
layout (binding = 5) uniform sampler2D samplerMetallicRroughness;
layout (binding = 6) uniform sampler2D samplerVelocity;
layout (binding = 7) uniform sampler2D samplerSoftRtImage;
layout (binding = 8) uniform sampler2D samplerRtImageDynamic;
layout (binding = 9) uniform sampler2D samplerRtImageStatic;
layout (binding = 10) uniform samplerCube samplerIrradiance;
layout (binding = 11) uniform samplerCube prefilteredMap;
layout (binding = 12) uniform sampler2D samplerBRDFLUT;

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

const float M_PI = 3.141592653589793;

vec3 GetPosFromDepth(vec2 screenPos){
  float depth = texture(samplerDepth, screenPos).x;
  vec4 clipPos = vec4( screenPos * 2.0f - 1.0f, depth, 1.0f);
  vec4 pixelPos = inverse(params.mProjView) * clipPos; 
  return (pixelPos.xyz/pixelPos.w);
}
float sq(float x) { return x*x; }

vec2 rotate(vec2 v, float a) {
    a *= M_PI / 180.f;
    float s = sin(a);
    float c = cos(a);
    mat2 m = mat2(c, -s, s, c);
    return m * v;
}

vec3 rotateXZ(vec3 v, float a) {
    vec3 res = v;
    res.xz = rotate(res.xz, a);
    return res;
}

vec3 toSky(vec3 v) {
    return rotateXZ(v, UboParams.envMapRotation);
}

vec3 fromSky(vec3 v) {
    return rotateXZ(v, -UboParams.envMapRotation);
}

struct PBRData {
    vec3 L;
    vec3 V;
    vec3 N;
    float dotNV;
    float dotNL;
    float dotLH;
    float dotNH;

    vec3 albedo;
    float metallic;
    float roughness;
};

// Normal Distribution function --------------------------------------
float D_GGX(float dotNH, float roughness)
{
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
    return (alpha2)/(M_PI * denom*denom);
}

// Geometric Shadowing function --------------------------------------
float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;
    float GL = dotNL / (dotNL * (1.0 - k) + k);
    float GV = dotNV / (dotNV * (1.0 - k) + k);
    return GL * GV;
}

// Fresnel function ----------------------------------------------------
vec3 F_Schlick(float cosTheta, float metallic, vec3 albedo)
{
    vec3 F0 = mix(vec3(0.04), albedo, metallic);// * material.specular
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
    return F;
}

vec3 getIBLContribution(
    vec3 diffuseColor,
    vec3 specularColor,
    float NdotV,
    float roughness,
    vec3 N,
    vec3 reflection)
{
    float lod = (roughness * UboParams.prefilteredCubeMipLevels);
    // retrieve a scale and bias to F0. See [1], Figure 3
    vec3 brdf = (texture(samplerBRDFLUT, vec2(NdotV, 1.0 - roughness))).rgb;
    vec3 diffuseLight = texture(samplerIrradiance, toSky(N)).rgb;

    vec3 specularLight = textureLod(prefilteredMap, toSky(reflection), lod).rgb;

    vec3 diffuse = diffuseLight * diffuseColor;
    vec3 specular = specularLight * (specularColor * brdf.x + brdf.y);

    return diffuse + specular;
}

// Specular BRDF composition --------------------------------------------

vec3 BRDF(PBRData d, float shadow_visibility)
{
    vec3 f0 = vec3(0.04);

    vec3 diffuseColor = d.albedo * (vec3(1.0) - f0);
    diffuseColor *= 1.0 - d.metallic;

    vec3 specularColor = mix(f0, d.albedo, d.metallic);


    // Light color fixed
    const vec3 lightColor = vec3(1.f);

    vec3 color = vec3(0.0);

    if (d.dotNL > 0.0)
    {
        float rroughness = max(0.05, d.roughness);
        // D = Normal distribution (Distribution of the microfacets)
        float D = D_GGX(d.dotNH, d.roughness);
        // G = Geometric shadowing term (Microfacets shadowing)
        float G = G_SchlicksmithGGX(d.dotNL, d.dotNV, rroughness);
        // F = Fresnel factor (Reflectance depending on angle of incidence)
        vec3 F = F_Schlick(d.dotNV, d.metallic, d.albedo);

        vec3 diffuseContrib = (1.0 - F) * diffuseColor;// / PI;
        vec3 spec = D * F * G / (4.0 * d.dotNL * d.dotNV);

        color += UboParams.lights[0].radius_lightDist_dummies.z * (diffuseContrib + spec) * d.dotNL * lightColor * shadow_visibility;
    }

    vec3 reflection = -normalize(reflect(d.V, d.N));
    reflection.y *= -1.0f;
    color += getIBLContribution(diffuseColor, specularColor, d.dotNV, d.roughness, d.N, reflection)
        * (1 - (1 - shadow_visibility) * (1 - UboParams.IBLShadowedRatio));

    return color;
}

void main() 
{
    vec2 uv = gl_FragCoord.xy / params.screenSize;
    vec3 fragPos = GetPosFromDepth(uv);
	vec3 normal = texture(samplerNormal, uv).rgb;
	vec4 albedo = texture(samplerAlbedo, uv);
    float depth = texture(samplerDepth, uv).x;
    vec2 metRough = texture(samplerMetallicRroughness, uv).xy;

    float softShadow = texture(samplerSoftRtImage, uv).x;

    // Specular contribution
    PBRData pbrData;
    mat4 mViewInv = inverse(UboParams.View);
    pbrData.V = normalize((mViewInv * vec4(0., 0., 0., 1.)).xyz - fragPos);    
    //spmething about sky
    if (depth == 1.) {
        outFragcolor = textureLod(prefilteredMap, pbrData.V, 0);
        // if ((Params.debugFlags & 4) == 0)
        //     out_fragColor = vec4(tonemapLottes(out_fragColor.xyz * UboParams.exposure), 1.);
        return;
    }

    pbrData.N = normal;
    // Normal mapping is missing here
    pbrData.albedo = albedo.xyz;
    pbrData.metallic = metRough.x;
    pbrData.roughness = metRough.y;
    
    // pbrData.N = normalize(transpose(mat3(UboParams.View)) * pbrData.N);
    pbrData.L = UboParams.lights[0].dir.xyz;

    // Precalculate vectors and dot products
    vec3 H = normalize (pbrData.V + pbrData.L);
    pbrData.dotNV = clamp(abs(dot(pbrData.N, pbrData.V)), 0.001, 1.0);
    pbrData.dotNL = clamp(dot(pbrData.N, pbrData.L), 0.0, 1.0);
    pbrData.dotLH = clamp(dot(pbrData.L, H), 0.0, 1.0);
    pbrData.dotNH = clamp(dot(pbrData.N, H), 0.0, 1.0);

    // Specular contribution
    vec3 color = BRDF(pbrData, softShadow);

    switch (int(UboParams.m_time_gbuffer_index.w)) {
    case 0:
        if(UboParams.settings.y == 1)
            outFragcolor = vec4(color,1.0f);
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
        //outFragcolor = vec4(depth, 0.0f, 0.0f, 1.0f);
        outFragcolor = vec4(metRough.x, metRough.y, 0.0f, 1.0f);
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