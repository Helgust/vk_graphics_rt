#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) out float out_fragColor;

layout (location = 0 ) in VS_OUT
{
    vec2 texCoord;
    vec4 pos;
} surf;

layout(binding = 0, set = 0) uniform AppData
{
    UniformParams UboParams;
};


void main()
{
    vec3 lightVec = surf.pos.xyz - UboParams.lights[0].pos.xyz;
    out_fragColor = length(lightVec);
    //out_fragColor = vec4(1.0f, 0.0f, 1.0f, 1.0f);
}