#ifndef VK_GRAPHICS_BASIC_COMMON_H
#define VK_GRAPHICS_BASIC_COMMON_H

#ifdef __cplusplus
#include <LiteMath.h>
using LiteMath::uint2;
using LiteMath::float2;
using LiteMath::float3;
using LiteMath::float4;
using LiteMath::float4x4;
using LiteMath::make_float2;
using LiteMath::make_float4;
using LiteMath::make_int4;
using LiteMath::int4;

typedef unsigned int uint;
typedef uint2        uvec2;
typedef float4       vec4;
typedef float3       vec3;
typedef float2       vec2;
typedef float4x4     mat4;
typedef int4        ivec4;
#endif

#include "lights_common.h"
struct UniformParams
{
  Light lights[1];
  vec4  baseColor;
  vec4 m_time_gbuffer_index; // xy dummy z time w gbuffer_index
  vec4 jitterOffset; // xy curr jitter zw prev jitter
  mat4 projView;
  mat4 invProjView;
  mat4 prevProjView;
  mat4 invPrevProjView;
  mat4 PrevVecMat;
  mat4 View;
  float prefilteredCubeMipLevels;
  float exposure;
  float IBLShadowedRatio;
  float envMapRotation;
  ivec4 settings;// x taa y softShadow zw resolution
};

struct MaterialData_pbrMR
{
  vec4 baseColor;

  float metallic;
  float roughness;
  int baseColorTexId;
  int metallicRoughnessTexId;

  vec3 emissionColor;
  int emissionTexId;

  int normalTexId;
  int occlusionTexId;
  float alphaCutoff;
  int alphaMode;
};
const float JITTER_SCALE = 1.2f;

#endif //VK_GRAPHICS_BASIC_COMMON_H
