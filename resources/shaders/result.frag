#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require
//#extension GL_EXT_debug_printf : enable

#include "common.h"

layout(location = 0) out vec4 outColor;

layout (binding = 0, set = 0) uniform sampler2D resultText;

layout (location = 0 ) in VS_OUT
{
  vec2 texCoord;
} surf;

void main()
{
  outColor = vec4(textureLod(resultText,surf.texCoord,0).xyz,1.0);
}