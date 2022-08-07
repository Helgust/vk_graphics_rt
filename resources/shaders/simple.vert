#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_debug_printf : enable

#include "unpack_attributes.h"
#include "common.h"


layout(location = 0) in vec4 vPosNorm;
layout(location = 1) in vec4 vTexCoordAndTang;

layout(binding = 0, set = 0) uniform AppData
{
    UniformParams UboParams;
};

layout(push_constant) uniform params_t
{
    mat4 mProjView;
    mat4 mModel;
    mat4 lightMatrix;
    vec4 color;
    vec2 screenSize;
} params;


layout (location = 0 ) out VS_OUT
{
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;
    vec4 currClipSpacePos;
    vec4 prevClipSpacePos;

} vOut;

out gl_PerVertex { vec4 gl_Position; };
void main(void)
{
    const vec4 wNorm = vec4(DecodeNormal(floatBitsToInt(vPosNorm.w)),         0.0f);
    const vec4 wTang = vec4(DecodeNormal(floatBitsToInt(vTexCoordAndTang.z)), 0.0f);

    vOut.wPos     = (params.mModel * vec4(vPosNorm.xyz, 1.0f)).xyz;
    vOut.wNorm    = normalize(mat3(transpose(inverse(params.mModel))) * wNorm.xyz);
    vOut.wTangent = normalize(mat3(transpose(inverse(params.mModel))) * wTang.xyz);
    vOut.texCoord = vTexCoordAndTang.xy;

    vec4 clipSpacePos = params.mProjView * vec4(vOut.wPos, 1.0);
    vOut.currClipSpacePos = clipSpacePos;
    clipSpacePos += vec4(UboParams.m_jitter_time_gbuffer_index.xy * clipSpacePos.w, 0, 0);
    gl_Position = clipSpacePos;

    
    vOut.prevClipSpacePos = UboParams.prevProjView * vec4(vOut.wPos, 1.0);
    
}
