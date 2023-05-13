#ifndef RayTracer_UBO_H
#define RayTracer_UBO_H

#ifndef GLSL
#define LAYOUT_STD140
#include "LiteMath.h"
typedef LiteMath::float4x4 mat4;
typedef LiteMath::float2   vec2;
typedef LiteMath::float3   vec3;
typedef LiteMath::float4   vec4;
typedef LiteMath::int4   ivec4;
typedef LiteMath::uint4   uvec4;
typedef unsigned uint;
#else
#define MAXFLOAT 1e37f
#define M_PI          3.14159265358979323846f
#define M_TWOPI       6.28318530717958647692f
#define INV_PI        0.31830988618379067154f
#define INV_TWOPI     0.15915494309189533577f
#endif
#include "../../../../resources/shaders/lights_common.h"
struct RayTracer_UBO_Data
{
  mat4 m_invProjView;
  mat4 m_currProjView;
  mat4 m_prevProjView;
  mat4 m_invPrevProjView;
  mat4 m_invTransMat;
  vec4 m_camPos;
  vec4 m_vehPos;
  uvec4 needUpdate;
  ivec4 randomVal;
  uint m_height;
  uint m_width;
  uint m_pAccelStruct_capacity;
  uint m_pAccelStruct_size;
  Light lights[1];
  float time;
  uint dummy;
};

#endif

