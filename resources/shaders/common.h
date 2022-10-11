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

struct Light { //deprecated pos and radius
  vec4  dir;
  vec4  color;
  vec4  pos; 
  vec4 radius_lightDist_dummies; // x-radius, y-lightDist zw - dumy things //
};

struct UniformParams
{
  Light lights[2];
  vec4  baseColor;
  vec4 m_time_gbuffer_index; // xy dummy z time w gbuffer_index
  vec4 m_cur_prev_jiiter; // xy curr jitter zw prev jitter
  mat4 invProjView;
  mat4 prevProjView;
  mat4 invPrevProjView;
  ivec4 settings;// x taa y softShadow zw dummy
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
const float JITTER_SCALE = 1.1;

#endif //VK_GRAPHICS_BASIC_COMMON_H
