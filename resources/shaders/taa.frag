#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_debug_printf : enable

#include "common.h"

layout(location = 0) out vec4 outColor;

layout (binding = 0, set = 0) uniform sampler2D colorTex;
layout (binding = 1, set = 0) uniform sampler2D oldColorTex;

layout (location = 0 ) in VS_OUT
{
  vec2 texCoord;
} surf;

void main()
{
  vec3 currentFrame = textureLod(colorTex,surf.texCoord,0).xyz;
  vec3 prevFrame = textureLod(oldColorTex,surf.texCoord,0).xyz;
  //debugPrintfEXT("currentFrameColor = %1.2v3f prevFrameColor = %1.2v3f\n", currentFrame, prevFrame);
  vec3 c = mix(currentFrame,prevFrame,0.95);
  //vec3 c = mix(prevFrame,currentFrame,mix(0.05,0.6,0));
  outColor = vec4(c,1.0);
}
