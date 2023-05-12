#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_shader_draw_parameters  : enable
//#extension GL_EXT_debug_printf : enable

#include "unpack_attributes.h"
#include "common.h"


layout(location = 0) in vec4 vPosNorm;
layout(location = 1) in vec4 vTexCoordAndTang;

layout(binding = 0, set = 0) uniform AppData
{
    UniformParams UboParams;
};
layout(binding = 1, set = 0) buffer materialsBuf { MaterialData_pbrMR materials[]; };
layout(binding = 2, set = 0) buffer dynMaterialsBuf { MaterialData_pbrMR dynMaterials[]; };
layout(binding = 3, set = 0) buffer materialPerVertIdsBuf { uint materiaPerVertlIds[]; };
layout(binding = 4, set = 0) buffer dynMaterialPerVertIdsBuf { uint dynMaterialPerVertIds[]; };
layout(binding = 7, set = 0) buffer MeshInfos { uvec4 o[]; } infos;
layout(binding = 8, set = 0) buffer MaterialsID { uint matID[]; };

layout(push_constant) uniform params_t
{
    mat4 mModel;
    uint dynamicBit;
    uint meshID;
} params;


layout (location = 0 ) out VS_OUT
{
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;
    vec4 currClipSpacePos;
    vec4 prevClipSpacePos;
    vec3 color;
    flat uint materialId;

} vOut;

// out gl_PerVertex { vec4 gl_Position; };
void main(void)
{
    const vec4 wNorm = vec4(DecodeNormal(floatBitsToInt(vPosNorm.w)),         0.0f);
    const vec4 wTang = vec4(DecodeNormal(floatBitsToInt(vTexCoordAndTang.z)), 0.0f);

    vOut.wPos     = (params.mModel * vec4(vPosNorm.xyz, 1.0f)).xyz;
    vOut.wNorm    = normalize(mat3(transpose(inverse(params.mModel))) * wNorm.xyz);
    vOut.wTangent = normalize(mat3(transpose(inverse(params.mModel))) * wTang.xyz);
    vOut.texCoord = vTexCoordAndTang.xy;

    vec4 clipSpacePos = UboParams.projView * vec4(vOut.wPos, 1.0);
    //clipSpacePos += vec4(UboParams.m_jitter_time_gbuffer_index.xy * clipSpacePos.w, 0, 0);
    vOut.currClipSpacePos = clipSpacePos;
    gl_Position = clipSpacePos;
    if (params.dynamicBit.x != 1)
    {
        vOut.prevClipSpacePos = UboParams.prevProjView * vec4(vOut.wPos, 1.0);
        // vOut.color = materials[matIdx].baseColor.xyz;
        //vOut.materialId = materiaPerVertlIds[gl_BaseVertexARB];
    }
    else
    {
        vOut.prevClipSpacePos = UboParams.prevProjView * UboParams.PrevVecMat * vec4(vOut.wPos, 1.0);//Fixme
        // vOut.color = dynMaterials[matIdx].baseColor.xyz;
        //vOut.materialId = dynMaterialPerVertIds[gl_BaseVertexARB];
        vOut.prevClipSpacePos = UboParams.prevProjView * vec4(vOut.wPos, 1.0);
    }
    
    
}
