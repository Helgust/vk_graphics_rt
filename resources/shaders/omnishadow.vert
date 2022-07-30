#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"
#include "unpack_attributes.h"


layout(location = 0) in vec4 vPosNorm;
layout(location = 1) in vec4 vTexCoordAndTang;

layout(push_constant) uniform params_t
{
    mat4 mProjView;
    mat4 mModel;
    mat4 lightMatrix;
    vec4 color;
    vec2 screenSize; 
} pushParams;

layout(binding = 0, set = 0) uniform AppData
{
    UniformParams UboParams;
};

layout (location = 0 ) out VS_OUT
{
    vec2 texCoord;
    vec4 pos;
} vOut;

void main(void)
{
    vOut.texCoord = vTexCoordAndTang.xy;
    vOut.pos = vec4(vPosNorm.xyz, 1.0f);
    gl_Position   = pushParams.lightMatrix * pushParams.mModel * vec4(vPosNorm.xyz, 1.0);
}

