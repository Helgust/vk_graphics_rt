#include <vector>
#include <memory>
#include <limits>
#include <cassert>
#include <chrono>
#include <time.h>

#include "vk_copy.h"
#include "vk_context.h"
#include "vk_images.h"

#include "raytracing_generated.h"
#include "include/RayTracer_ubo.h"

#include "CrossRT.h"
ISceneObject* CreateVulkanRTX(VkDevice a_device, VkPhysicalDevice a_physDevice, uint32_t a_graphicsQId, std::shared_ptr<vk_utils::ICopyEngine> a_pCopyHelper,
                              uint32_t a_maxMeshes, uint32_t a_maxTotalVertices, uint32_t a_maxTotalPrimitives, uint32_t a_maxPrimitivesPerMesh,
                              bool build_as_add);

std::shared_ptr<RayTracer> CreateRayTracer_Generated(uint32_t a_width, uint32_t a_height, vk_utils::VulkanContext a_ctx, size_t a_maxThreadsGenerated)
{
  auto pObj = std::make_shared<RayTracer_Generated>(a_width, a_height);
  pObj->SetVulkanContext(a_ctx);
  pObj->InitVulkanObjects(a_ctx.device, a_ctx.physicalDevice, a_maxThreadsGenerated);
  return pObj;
}

static uint32_t ComputeReductionSteps(uint32_t whole_size, uint32_t wg_size)
{
  uint32_t steps = 0;
  while (whole_size > 1)
  {
    steps++;
    whole_size = (whole_size + wg_size - 1) / wg_size;
  }
  return steps;
}

constexpr uint32_t KGEN_FLAG_RETURN            = 1;
constexpr uint32_t KGEN_FLAG_BREAK             = 2;
constexpr uint32_t KGEN_FLAG_DONT_SET_EXIT     = 4;
constexpr uint32_t KGEN_FLAG_SET_EXIT_NEGATIVE = 8;
constexpr uint32_t KGEN_REDUCTION_LAST_STEP    = 16;

void RayTracer_Generated::InitVulkanObjects(VkDevice a_device, VkPhysicalDevice a_physicalDevice, size_t a_maxThreadsCount)
{
  physicalDevice = a_physicalDevice;
  device         = a_device;
  InitHelpers();
  InitBuffers(a_maxThreadsCount, true);
  InitKernels(".spv");
  AllocateAllDescriptorSets();

  auto queueAllFID = vk_utils::getQueueFamilyIndex(physicalDevice, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT);
  //@TODO: calculate these somehow?
  uint32_t maxMeshes = 1024;
  uint32_t maxTotalVertices = 1'000'000;
  uint32_t maxTotalPrimitives = 1'000'000;
  uint32_t maxPrimitivesPerMesh = 200'000;
  m_pAccelStruct = std::shared_ptr<ISceneObject>(CreateVulkanRTX(a_device, a_physicalDevice, queueAllFID, m_ctx.pCopyHelper,
                                                             maxMeshes, maxTotalVertices, maxTotalPrimitives, maxPrimitivesPerMesh, true),
                                                            [](ISceneObject *p) { DeleteSceneRT(p); } );
}

void RayTracer_Generated::UpdatePlainMembers(std::shared_ptr<vk_utils::ICopyEngine> a_pCopyEngine, float a_time, Light a_light, LiteMath::uint4 a_needUpdate, LiteMath::float4 a_vehPos)
{
  const size_t maxAllowedSize = std::numeric_limits<uint32_t>::max();

  m_uboData.m_invProjView = m_invProjView;
  m_uboData.m_currProjView = m_currProjView;
  m_uboData.m_prevProjView = m_prevProjView;
  m_uboData.m_invPrevProjView = m_invPrevProjView;
  m_uboData.m_invTransMat = m_invTransMat;
  m_uboData.m_camPos = m_camPos;
  m_uboData.m_height = m_height;
  m_uboData.m_width = m_width;
  m_uboData.time = a_time;
  m_uboData.lights[0] = a_light;
  m_uboData.needUpdate = a_needUpdate;
  m_uboData.m_vehPos = a_vehPos;
  srand((unsigned) time(NULL));
  for (int i = 0; i < 4; i++)
  {
    m_uboData.randomVal[i] = rand()%256;
  }
  a_pCopyEngine->UpdateBuffer(m_classDataBuffer, 0, &m_uboData, sizeof(m_uboData));
}


void RayTracer_Generated::UpdateVectorMembers(std::shared_ptr<vk_utils::ICopyEngine> a_pCopyEngine)
{
}

void RayTracer_Generated::UpdateTextureMembers(std::shared_ptr<vk_utils::ICopyEngine> a_pCopyEngine)
{
}

void RayTracer_Generated::CastSingleRayMegaCmd(uint32_t tidX, uint32_t tidY, uint32_t* out_color, uint32_t a_IsStaticPass, uint32_t a_isAO)
{
  uint32_t blockSizeX = 256;
  uint32_t blockSizeY = 1;
  uint32_t blockSizeZ = 1;

  struct KernelArgsPC
  {
    uint32_t m_sizeX;
    uint32_t m_sizeY;
    uint32_t m_sizeZ;
    uint32_t m_tFlags;
    uint32_t m_tIsStaticPass;
    uint32_t m_isAO;
  } pcData;

  uint32_t sizeX  = uint32_t(tidX);
  uint32_t sizeY  = uint32_t(tidY);
  uint32_t sizeZ  = uint32_t(1);

  pcData.m_sizeX  = tidX;
  pcData.m_sizeY  = tidY;
  pcData.m_sizeZ  = 1;
  pcData.m_tFlags = m_currThreadFlags;
  pcData.m_tIsStaticPass = a_IsStaticPass;
  pcData.m_isAO = a_isAO;

  vkCmdPushConstants(m_currCmdBuffer, CastSingleRayMegaLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(KernelArgsPC), &pcData);

  vkCmdBindPipeline(m_currCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, CastSingleRayMegaPipeline);
  vkCmdDispatch    (m_currCmdBuffer, (sizeX + blockSizeX - 1) / blockSizeX, (sizeY + blockSizeY - 1) / blockSizeY, (sizeZ + blockSizeZ - 1) / blockSizeZ);

}


void RayTracer_Generated::copyKernelFloatCmd(uint32_t length)
{
  uint32_t blockSizeX = MEMCPY_BLOCK_SIZE;

  vkCmdBindPipeline(m_currCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, copyKernelFloatPipeline);
  vkCmdPushConstants(m_currCmdBuffer, copyKernelFloatLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &length);
  vkCmdDispatch(m_currCmdBuffer, (length + blockSizeX - 1) / blockSizeX, 1, 1);
}

VkBufferMemoryBarrier RayTracer_Generated::BarrierForClearFlags(VkBuffer a_buffer)
{
  VkBufferMemoryBarrier bar = {};
  bar.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
  bar.pNext               = NULL;
  bar.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
  bar.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;
  bar.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  bar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  bar.buffer              = a_buffer;
  bar.offset              = 0;
  bar.size                = VK_WHOLE_SIZE;
  return bar;
}

VkBufferMemoryBarrier RayTracer_Generated::BarrierForSingleBuffer(VkBuffer a_buffer)
{
  VkBufferMemoryBarrier bar = {};
  bar.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
  bar.pNext               = NULL;
  bar.srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
  bar.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;
  bar.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  bar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  bar.buffer              = a_buffer;
  bar.offset              = 0;
  bar.size                = VK_WHOLE_SIZE;
  return bar;
}

VkImageMemoryBarrier RayTracer_Generated::BarrierForSingleImage(VkImage a_image, uint32_t srcFlag, uint32_t dstFlag,
VkImageLayout oldLayout,
VkImageLayout newLayout,
uint32_t a_aspectMask)
{
  VkImageMemoryBarrier bar = {};
  bar.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  bar.pNext               = NULL;
  bar.srcAccessMask       = srcFlag;
  bar.dstAccessMask       = dstFlag;
  bar.oldLayout           = oldLayout;
  bar.newLayout           = newLayout;
  bar.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  bar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  bar.image              = a_image;

  bar.subresourceRange.aspectMask     = a_aspectMask;
  bar.subresourceRange.baseMipLevel   = 0;
  bar.subresourceRange.baseArrayLayer = 0;
  bar.subresourceRange.layerCount     = 1;
  bar.subresourceRange.levelCount     = 1;
  return bar;
}

void RayTracer_Generated::BarriersForSeveralBuffers(VkBuffer* a_inBuffers, VkBufferMemoryBarrier* a_outBarriers, uint32_t a_buffersNum)
{
  for(uint32_t i=0; i<a_buffersNum;i++)
  {
    a_outBarriers[i].sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    a_outBarriers[i].pNext               = NULL;
    a_outBarriers[i].srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
    a_outBarriers[i].dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;
    a_outBarriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    a_outBarriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    a_outBarriers[i].buffer              = a_inBuffers[i];
    a_outBarriers[i].offset              = 0;
    a_outBarriers[i].size                = VK_WHOLE_SIZE;
  }
}

void RayTracer_Generated::CastSingleRayCmd(VkCommandBuffer a_commandBuffer,
 uint32_t tidX, uint32_t tidY, uint32_t* out_color, VkImage a_image, uint32_t a_isStaticPass, uint32_t a_isAO)
{
  m_currCmdBuffer = a_commandBuffer;
  VkMemoryBarrier memoryBarrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT };

  // VkImageMemoryBarrier imageBarrier;
  // imageBarrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  // imageBarrier.pNext               = nullptr;
  // imageBarrier.srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
  // imageBarrier.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;
  // imageBarrier.oldLayout           = VK_IMAGE_LAYOUT_GENERAL;
  // imageBarrier.newLayout           = VK_IMAGE_LAYOUT_GENERAL;
  // imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  // imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  // imageBarrier.image               = a_image;

  // imageBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
  // imageBarrier.subresourceRange.baseMipLevel   = 0;
  // imageBarrier.subresourceRange.baseArrayLayer = 0;
  // imageBarrier.subresourceRange.layerCount     = 1;
  // imageBarrier.subresourceRange.levelCount     = 1;

  vkCmdBindDescriptorSets(a_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, CastSingleRayMegaLayout, 0, 1, &m_allGeneratedDS[0], 0, nullptr);
  CastSingleRayMegaCmd(tidX, tidY, out_color, a_isStaticPass, a_isAO);
}


