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

// struct Light 
// {
//   vec3 dir;
//   uint color;
//   float rad;
// };

//For buggy
//const Light l1 = { {0.3f, 1.0f, -0.5f}, 0xffffffff, 5.5f};
// const Light l1 = { {0.0f, 0.4f, -1.0f}, 0xffffffff, 0.8f};

//buster_drone
//const Light l1 = { {40.0f,10.0f,150.0f,1.0f}, 0xff000000, 100000.0f };
//const Light l2 = { {40.0f,-40.0f,120.0f,1.0f}, 0x0000ff00, 100000.0f };
// const Light m_lights[1] = {l1};
const int samples_cnt = 16;
const int light_cnt = 1;
const float rayMax = 1000.0f;
//const float light_dist = 20.0f;
bool soft_shadow = true;
bool first_run = true;
bool debug_light_pos = false;
int depth_steps = 30;
float step_size = 0.01f;
float depth_eps = 0.0020f;
float point_eps = 0.000f;
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
  uint dynamicBit;
  uint dummy;
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

// vec3 PointOnLight(Light m_light,float st)
// {
//   float theta = st*2*M_PI;
//   float phi = acos(2*st - 1);
//   //float r =  sqrt(stepAndOutputRNGFloat(st))*light_rad;
//   return vec3(m_light.pos.x + m_light.rad*sin(phi)*cos(theta),m_light.pos.y + m_light.rad*sin(phi)*sin(theta),m_light.pos.z + m_light.rad*cos(phi));
// }
bool reprojection = true;
vec2 PointOnDisk(Light m_light, float st, vec2 samplePos)
{
  float theta = st*2*M_PI;
  float cosTheta = cos(theta);
  float sinTheta = sin(theta);
  vec2 result = vec2(0.0f, 0.0f);
  result.x = samplePos.x * cosTheta - samplePos.y * sinTheta;
  result.y = samplePos.x * sinTheta + samplePos.y * cosTheta;
  result *= m_light.radius_lightDist_dummies.x;
  return result;
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

const vec2 BlueNoiseInDisk[64] = vec2[64](
    vec2(0.478712,0.875764),
    vec2(-0.337956,-0.793959),
    vec2(-0.955259,-0.028164),
    vec2(0.864527,0.325689),
    vec2(0.209342,-0.395657),
    vec2(-0.106779,0.672585),
    vec2(0.156213,0.235113),
    vec2(-0.413644,-0.082856),
    vec2(-0.415667,0.323909),
    vec2(0.141896,-0.939980),
    vec2(0.954932,-0.182516),
    vec2(-0.766184,0.410799),
    vec2(-0.434912,-0.458845),
    vec2(0.415242,-0.078724),
    vec2(0.728335,-0.491777),
    vec2(-0.058086,-0.066401),
    vec2(0.202990,0.686837),
    vec2(-0.808362,-0.556402),
    vec2(0.507386,-0.640839),
    vec2(-0.723494,-0.229240),
    vec2(0.489740,0.317826),
    vec2(-0.622663,0.765301),
    vec2(-0.010640,0.929347),
    vec2(0.663146,0.647618),
    vec2(-0.096674,-0.413835),
    vec2(0.525945,-0.321063),
    vec2(-0.122533,0.366019),
    vec2(0.195235,-0.687983),
    vec2(-0.563203,0.098748),
    vec2(0.418563,0.561335),
    vec2(-0.378595,0.800367),
    vec2(0.826922,0.001024),
    vec2(-0.085372,-0.766651),
    vec2(-0.921920,0.183673),
    vec2(-0.590008,-0.721799),
    vec2(0.167751,-0.164393),
    vec2(0.032961,-0.562530),
    vec2(0.632900,-0.107059),
    vec2(-0.464080,0.569669),
    vec2(-0.173676,-0.958758),
    vec2(-0.242648,-0.234303),
    vec2(-0.275362,0.157163),
    vec2(0.382295,-0.795131),
    vec2(0.562955,0.115562),
    vec2(0.190586,0.470121),
    vec2(0.770764,-0.297576),
    vec2(0.237281,0.931050),
    vec2(-0.666642,-0.455871),
    vec2(-0.905649,-0.298379),
    vec2(0.339520,0.157829),
    vec2(0.701438,-0.704100),
    vec2(-0.062758,0.160346),
    vec2(-0.220674,0.957141),
    vec2(0.642692,0.432706),
    vec2(-0.773390,-0.015272),
    vec2(-0.671467,0.246880),
    vec2(0.158051,0.062859),
    vec2(0.806009,0.527232),
    vec2(-0.057620,-0.247071),
    vec2(0.333436,-0.516710),
    vec2(-0.550658,-0.315773),
    vec2(-0.652078,0.589846),
    vec2(0.008818,0.530556),
    vec2(-0.210004,0.519896) 
);

uint fakeOffset(uint x, uint y, uint pitch) { return y*pitch + x; }  // RTV pattern, for 2D threading

#define KGEN_FLAG_RETURN            1
#define KGEN_FLAG_BREAK             2
#define KGEN_FLAG_DONT_SET_EXIT     4
#define KGEN_FLAG_SET_EXIT_NEGATIVE 8
#define KGEN_REDUCTION_LAST_STEP    16

