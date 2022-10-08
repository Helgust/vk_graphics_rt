#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(push_constant) uniform params_t
{
    mat4 mProjView;
    mat4 mModel;
    mat4 lightMatrix;
    vec4 color;
    vec2 screenSize; 
} params;

layout (location = 0) out vec4 outPosition;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec4 outAlbedo;
layout (location = 3) out vec2 outVelocity;

layout (location = 0 ) in VS_OUT
{
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;
    vec4 currPos;
    vec4 prevPos;
} surf;

layout(binding = 0, set = 0) uniform AppData
{
    UniformParams Params;
};

vec2 CalcVelocity(vec4 newPos, vec4 oldPos)
{
    // oldPos /= oldPos.w;
    // oldPos.xy = (oldPos.xy+1)/2.0f;
    // oldPos.y = 1 - oldPos.y;
    
    // newPos /= newPos.w;
    // newPos.xy = (newPos.xy+1)/2.0f;
    // newPos.y = 1 - newPos.y;
    
    // return (newPos - oldPos).xy;
    vec3 newPosNDC = newPos.xyz / newPos.w;
    vec3 oldPosNDC = oldPos.xyz / oldPos.w;
    newPosNDC.xy = (newPosNDC.xy * 0.5f + 0.5f);
    oldPosNDC.xy = (oldPosNDC.xy * 0.5f + 0.5f);
    vec2 velocity =  oldPosNDC.xy - newPosNDC.xy;
    return velocity;
}

void main()
{
    outAlbedo = params.color;
    outNormal = vec4(surf.wNorm, 1.0f);
    outPosition = vec4(surf.wPos, 1.0f);
    //outVelocity = vec4(CalcVelocity(surf.currPos, surf.prevPos), 0.0f, 1.0f);
    outVelocity = CalcVelocity(surf.currPos, surf.prevPos);
    //outVelocity -= (Params.m_cur_prev_jiiter.xy / 1024.0f);
    //outVelocity -= (Params.m_cur_prev_jiiter.zw / 1024.0f);
}