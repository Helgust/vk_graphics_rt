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
const vec3 m_ambient_color = vec3(1.0f, 1.0f, 1.0f);
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
  float rad;
  float light_dist;
};

//For buggy
const Light l1 = { {0.0f,12.0f,0.0f,1.0f}, 0xffffffff, 2.5f, 60.0f};
const Light l2 = { {0.0f,110.0f,-20.0f,1.0f},0xff000000, 5.0f, 10.0f};

//buster_drone
//const Light l1 = { {40.0f,10.0f,150.0f,1.0f}, 0xff000000, 100000.0f };
//const Light l2 = { {40.0f,-40.0f,120.0f,1.0f}, 0x0000ff00, 100000.0f };
const Light m_lights[2] = { l1, l2 };
const int samples_cnt = 8;
const int light_cnt = 1;
//const float light_dist = 20.0f;
bool soft_shadow = true;
bool debug_light_pos = true;

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

// const float Q1 = 0.6180339887498948482;
// const float P2 = 1.324717957244746;
// const vec2 Q2 = vec2(1./P2, 1./P2/P2);
// const vec2 Q22 = vec2(2./pow(P2,4.), 1./pow(P2,6.));
// const vec2 Q23 = vec2(2./pow(P2,7.), 1./pow(P2,8.));
// const vec2 Q24 = vec2(3./pow(P2,9.), 5./pow(P2,10.));
// const vec2 Q25 = vec2(3./pow(P2,11.), 5./pow(P2,12.));

vec2 Nth_weyl(vec2 p0, int n) {
    
    //return fract(p0 + float(n)*vec2(0.754877669, 0.569840296));
    return floor(p0 + vec2(n*12664745, n*9560333)/exp2(24.));	// integer mul to avoid round-off
}



vec3 EyeRayDir(float x, float y, float w, float h, mat4 a_mViewProjInv) {
  vec4 pos = vec4(2.0f * (x + 0.5f) / w - 1.0f, 2.0f * (y + 0.5f) / h - 1.0f, 0.0f, 1.0f);

  pos = a_mViewProjInv * pos;
  pos /= abs(pos.w);
  //pos.y *= (-1.0f);

  return normalize(pos.xyz);
}

vec3 PointOnLight(Light m_light,float st)
{
  float theta = st*2*M_PI;
  float phi = acos(2*st - 1);
  //float r =  sqrt(stepAndOutputRNGFloat(st))*light_rad;
  return vec3(m_light.pos.x + m_light.rad*sin(phi)*cos(theta),m_light.pos.y + m_light.rad*sin(phi)*sin(theta),m_light.pos.z + m_light.rad*cos(phi));
}

// Hash Functions for GPU Rendering, Jarzynski et al.
// http://www.jcgt.org/published/0009/03/02/
vec3 random_pcg3d(uvec3 v) {
  v = v * 1664525u + 1013904223u;
  v.x += v.y*v.z; v.y += v.z*v.x; v.z += v.x*v.y;
  v ^= v >> 16u;
  v.x += v.y*v.z; v.y += v.z*v.x; v.z += v.x*v.y;
  return vec3(v) * (1.0/float(0xffffffffu));
}

uvec2 random_pcg2d(uvec2 v)
{
    v = v * 1664525u + 1013904223u;

    v.x += v.y * 1664525u;
    v.y += v.x * 1664525u;

    v = v ^ (v>>16u);

    v.x += v.y * 1664525u;
    v.y += v.x * 1664525u;

    v = v ^ (v>>16u);

    return v;
}

uint fakeOffset(uint x, uint y, uint pitch) { return y*pitch + x; }  // RTV pattern, for 2D threading

#define KGEN_FLAG_RETURN            1
#define KGEN_FLAG_BREAK             2
#define KGEN_FLAG_DONT_SET_EXIT     4
#define KGEN_FLAG_SET_EXIT_NEGATIVE 8
#define KGEN_REDUCTION_LAST_STEP    16

