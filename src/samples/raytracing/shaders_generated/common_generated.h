/////////////////////////////////////////////////////////////////////
/////////////  Required  Shader Features ////////////////////////////
/////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////
/////////////////// include files ///////////////////////////////////
/////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////
/////////////////// declarations in class ///////////////////////////
/////////////////////////////////////////////////////////////////////
#ifndef uint32_t
#define uint32_t uint
#endif
struct CRT_Hit 
{
  float    t;         ///< intersection distance from ray origin to object
  uint primId; 
  uint instId;
  uint geomId; 
  vec3 bars;   ///< use 4 most significant bits for geometry type; thay are zero for triangles 
  float    coords[4]; ///< custom intersection data; for triangles coords[0] and coords[1] stores baricentric coords (u,v)
};
const uint MAX_DEPTH = 2;
const vec4 m_ambient_color = vec4(0.2f, 0.2f, 0.2f, 0.2f);
const uint palette_size = 5;
const uint m_palette[4] = {
    0xffe6194b, 0xff3cb44b, 0xffffe119, 0xff0082c8
    /*0xfff58231,  0xff911eb4, 0xff46f0f0, 0xfff032e6,
    0xffd2f53c, 0xfffabebe, 0xff008080, 0xffe6beff,
    0xffaa6e28, 0xfffffac8, 0xff800000, 0xffaaffc3,
    0xff808000, 0xffffd8b1, 0xff000080, 0xff808080 */
  };


const float m_reflection[5] = { 0.1f, 0.75f, 0.0f, 0.0f, 0.0f };

struct Light 
{
  vec4 pos;
  uint color;
  float intensity;
};

//For buggy
const Light l1 = { {0.0f,20.0f,-30.0f,1.0f}, 0xff000000, 10.0f};
const Light l2 = { {-60.0f,110.0f,70.0f,1.0f}, 0xff000000, 100.0f};

//buster_drone
//const Light l1 = { {40.0f,10.0f,150.0f,1.0f}, 0xff000000, 100000.0f };
//const Light l2 = { {40.0f,-40.0f,120.0f,1.0f}, 0x0000ff00, 100000.0f };
const Light m_lights[2] = { l1, l2 };
const int samples_cnt = 10;
const float light_rad = 10;
bool soft_shadow = true;

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

struct MeshInfo
{
  uint indexOffset;
  uint vertexOffset;
};

struct Vertex {
  vec4 vertex;
  vec4 tangent;
};

#include "include/RayTracer_ubo.h"
/////////////////////////////////////////////////////////////////////
/////////////////// local functions /////////////////////////////////
/////////////////////////////////////////////////////////////////////

vec3 EyeRayDir(float x, float y, float w, float h, mat4 a_mViewProjInv) {
  vec4 pos = vec4(2.0f * (x + 0.5f) / w - 1.0f, 2.0f * (y + 0.5f) / h - 1.0f, 0.0f, 1.0f);

  pos = a_mViewProjInv * pos;
  pos /= pos.w;

  //  pos.y *= (-1.0f);

  return normalize(pos.xyz);
}

// Random number generation using pcg32i_random_t, using inc = 1. Our random state is a uint.
uint stepRNG(uint rngState)
{
  return rngState * 747796405 + 1;
}

// Steps the RNG and returns a floating-point value between 0 and 1 inclusive.
float stepAndOutputRNGFloat(uint rngState)
{
  // Condensed version of pcg_output_rxs_m_xs_32_32, with simple conversion to floating-point [0,1].
  rngState  = stepRNG(rngState);
  uint word = ((rngState >> ((rngState >> 28) + 4)) ^ rngState) * 277803737;
  word      = (word >> 22) ^ word;
  return float(word) / 4294967295.0f;
}

vec3 PointOnLight(Light m_light,float st)
{
  float theta = st*2*M_PI;
  float phi = acos(2*st - 1);
  //float r =  sqrt(stepAndOutputRNGFloat(st))*light_rad;
  return vec3(m_light.pos.x + light_rad*sin(phi)*cos(theta),m_light.pos.y + light_rad*sin(phi)*sin(theta),m_light.pos.z + light_rad*cos(phi));
}

vec3 PointOnLightOld(Light m_light,uint st)
{
  float theta = stepAndOutputRNGFloat(st)*2*M_PI;
  float phi = acos(2*stepAndOutputRNGFloat(st) - 1);
  //float r =  sqrt(stepAndOutputRNGFloat(st))*light_rad;
  return vec3(m_light.pos.x + light_rad*sin(phi)*cos(theta),m_light.pos.y + light_rad*sin(phi)*sin(theta),m_light.pos.z + light_rad*cos(phi));
}


uint fakeOffset(uint x, uint y, uint pitch) { return y*pitch + x; }  // RTV pattern, for 2D threading

#define KGEN_FLAG_RETURN            1
#define KGEN_FLAG_BREAK             2
#define KGEN_FLAG_DONT_SET_EXIT     4
#define KGEN_FLAG_SET_EXIT_NEGATIVE 8
#define KGEN_REDUCTION_LAST_STEP    16

