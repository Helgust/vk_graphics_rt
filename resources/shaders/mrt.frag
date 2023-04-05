#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "common.h"

layout(push_constant) uniform params_t
{
    mat4 mProjView;
    mat4 mModel;
    mat4 lightMatrix;
    vec4 color;
    vec4 vehiclePos;
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
    vec3 color;
    flat uint materialId;
} surf;

layout(binding = 0, set = 0) uniform AppData
{
    UniformParams UboParams;
};

layout(binding = 1, set = 0) buffer materialsBuf { MaterialData_pbrMR materials[]; };
layout(binding = 3, set = 0) uniform sampler2D textures[];

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
    velocity -= (UboParams.m_cur_prev_jiiter.xy / params.screenSize.x);
    velocity -= (UboParams.m_cur_prev_jiiter.zw / params.screenSize.x);
    return velocity;
}

void main()
{
    vec4 albedo = vec4(surf.color,1);
    if (materials[uint(surf.materialId)].baseColorTexId != -1)
    {
        albedo = texture(textures[materials[uint(surf.materialId)].baseColorTexId], surf.texCoord);
    }
    if (albedo.a < 0.5)
        discard;
    outAlbedo = albedo;
    outNormal = vec4(surf.wNorm, 1.0f);
    outPosition = vec4(surf.wPos, 1.0f);
    //outVelocity = vec4(CalcVelocity(surf.currPos, surf.prevPos), 0.0f, 1.0f);
    outVelocity = CalcVelocity(surf.currPos, surf.prevPos);
}