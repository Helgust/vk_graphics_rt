#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_debug_printf : enable

#include "common.h"

layout(push_constant) uniform params_t
{
    mat4 mModel;
    uint dynamicBit;
    uint meshID;
} params;

layout (location = 0) out vec4 outPosition;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec4 outAlbedo;
layout (location = 3) out vec2 outVelocity;
layout (location = 4) out vec2 outMetRough;

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
layout(binding = 7, set = 0) buffer MeshInfos { uvec4 o[]; } infos;
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
    velocity -= (UboParams.m_cur_prev_jiiter.xy / UboParams.settings.zw);
    velocity -= (UboParams.m_cur_prev_jiiter.zw / UboParams.settings.zw);
    return velocity;
}

void main()
{
    const uint offset = infos.o[params.meshID].x;
    const uint matIdx = matID[(offset / 3) + gl_PrimitiveID];
    //uint matIdx = surf.materialId;
    //debugPrintfEXT("gl_PrimitiveID%d, matIdx%d, offset%d\n",gl_PrimitiveID, matIdx, offset);
    vec4 albedo = vec4(surf.color,1);
    vec2 metRough;
    if(params.dynamicBit != 1)
    {
        MaterialData_pbrMR material = materials[matIdx];
        albedo = texture(textures[material.baseColorTexId], surf.texCoord);
        metRough = vec2(material.metallic, material.roughness);
        if (material.metallicRoughnessTexId >= 0)
            metRough = texture(textures[material.metallicRoughnessTexId], surf.texCoord).bg;

        if (material.baseColorTexId >= 0) {
            float alpha = texture(textures[material.baseColorTexId], surf.texCoord).a;

            if (material.alphaMode == 1) {
                if (alpha < material.alphaCutoff) {
                    discard;
                }
            }
        }
        // if(albedo.a < 0.5f)
        //     discard;
    }
    else
    {
        MaterialData_pbrMR dynMaterial = dynMaterials[matIdx];
        albedo = texture(dynTextures[dynMaterial.baseColorTexId], surf.texCoord);
        metRough = vec2(dynMaterial.metallic, dynMaterial.roughness);
        if (dynMaterial.metallicRoughnessTexId >= 0)
            metRough = texture(dynTextures[dynMaterial.metallicRoughnessTexId], surf.texCoord).bg;

        if (dynMaterial.baseColorTexId >= 0) {
            float alpha = texture(dynTextures[dynMaterial.baseColorTexId], surf.texCoord).a;

            if (dynMaterial.alphaMode == 1) {
                if (alpha < dynMaterial.alphaCutoff) {
                    discard;
                }
            }
        }
        // if(albedo.a < 0.5f)
        //     discard;
    }

    outAlbedo = albedo;
    outNormal = vec4(surf.wNorm, 1.0f);
    outPosition = vec4(surf.wPos, 1.0f);
    //outVelocity = vec4(CalcVelocity(surf.currPos, surf.prevPos), 0.0f, 1.0f);
    outVelocity = CalcVelocity(surf.currPos, surf.prevPos);
    outMetRough = metRough;
}