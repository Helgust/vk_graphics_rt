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
    uint dynamicBit;
    uint instanceID; 
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
layout(binding = 2, set = 0) buffer dynMaterialsBuf { MaterialData_pbrMR dynMaterials[]; };
layout(binding = 5, set = 0) uniform sampler2D textures[];
layout(binding = 6, set = 0) uniform sampler2D dynTextures[];
layout(binding = 7, set = 0) buffer MeshInfos { uvec2 o[]; } infos;
layout(binding = 8, set = 0) buffer MaterialsID { uint matID[]; };

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
    const uint offset = infos.o[params.instanceID].x;
    const uint matIdx = matID[(offset / 3) + gl_PrimitiveID];
    vec4 albedo = vec4(surf.color,1);

    if(params.dynamicBit != 1)
    {
        MaterialData_pbrMR material = materials[matIdx];
        albedo = texture(textures[material.baseColorTexId], surf.texCoord);
    }
    else
    {
        MaterialData_pbrMR dynMaterial = dynMaterials[matIdx];
        albedo = texture(dynTextures[dynMaterial.baseColorTexId], surf.texCoord);
    }
        
    if (albedo.a < 0.5)
        discard;
    outAlbedo = albedo;
    outNormal = vec4(surf.wNorm, 1.0f);
    outPosition = vec4(surf.wPos, 1.0f);
    //outVelocity = vec4(CalcVelocity(surf.currPos, surf.prevPos), 0.0f, 1.0f);
    outVelocity = CalcVelocity(surf.currPos, surf.prevPos);
}