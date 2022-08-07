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

typedef unsigned int uint;
typedef uint2        uvec2;
typedef float4       vec4;
typedef float3       vec3;
typedef float2       vec2;
typedef float4x4     mat4;
#endif

struct Light {
		vec4  pos;
		vec4  color;
		vec4 radius_dummies; // x-radius, yzw - dumy things
	};

struct UniformParams
{
  Light lights[2];
  vec4  baseColor;
  vec4 m_jitter_time_gbuffer_index; // xy jitter, z time w gbuffer_index
  mat4 prevProjView;
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
const float JITTER_SCALE = 1.0;

#endif //VK_GRAPHICS_BASIC_COMMON_H
