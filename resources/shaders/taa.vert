#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require
#include "unpack_attributes.h"
#include "common.h"

layout(binding = 0, set = 0) uniform AppData
{
    UniformParams UboParams;
};

layout(push_constant) uniform params_t
{
    mat4 mModel;
    uint dynamicBit;
    uint meshID;
} params;

layout (location = 0 ) out VS_OUT
{
  vec2 texCoord;
} vOut;

void main() {
  vec2 xy = gl_VertexIndex == 0 ? vec2(-1, -1) : (gl_VertexIndex == 1 ? vec2(3, -1) : vec2(-1, 3));
  gl_Position   = vec4(xy*vec2(1,1), 0, 1);
  vOut.texCoord = xy * 0.5 + 0.5;
}