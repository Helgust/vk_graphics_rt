#include <vector>
#include <array>
#include <memory>
#include <limits>

#include <cassert>
#include "vk_copy.h"
#include "vk_context.h"

#include "raytracing_generated.h"
#include "include/RayTracer_ubo.h"

static uint32_t ComputeReductionAuxBufferElements(uint32_t whole_size, uint32_t wg_size)
{
  uint32_t sizeTotal = 0;
  while (whole_size > 1)
  {
    whole_size  = (whole_size + wg_size - 1) / wg_size;
    sizeTotal  += std::max<uint32_t>(whole_size, 1);
  }
  return sizeTotal;
}

VkBufferUsageFlags RayTracer_Generated::GetAdditionalFlagsForUBO() const
{
  return 0;
}

RayTracer_Generated::~RayTracer_Generated()
{
  m_pMaker = nullptr;
  vkDestroyDescriptorSetLayout(device, CastSingleRayMegaDSLayout, nullptr);
  CastSingleRayMegaDSLayout = VK_NULL_HANDLE;

  vkDestroyPipeline(device, CastSingleRayMegaPipeline, nullptr);
  vkDestroyPipelineLayout(device, CastSingleRayMegaLayout, nullptr);
  CastSingleRayMegaLayout   = VK_NULL_HANDLE;
  CastSingleRayMegaPipeline = VK_NULL_HANDLE;
  vkDestroyDescriptorSetLayout(device, copyKernelFloatDSLayout, nullptr);
  vkDestroyDescriptorPool(device, m_dsPool, NULL); m_dsPool = VK_NULL_HANDLE;

  vkDestroyBuffer(device, CastSingleRay_local.rayDirAndFarBuffer, nullptr);
  vkDestroyBuffer(device, CastSingleRay_local.rayPosAndNearBuffer, nullptr);


  vkDestroyBuffer(device, m_classDataBuffer, nullptr);


  FreeAllAllocations(m_allMems);
}

void RayTracer_Generated::InitHelpers()
{
  vkGetPhysicalDeviceProperties(physicalDevice, &m_devProps);
  m_pMaker = std::make_unique<vk_utils::ComputePipelineMaker>();
}

VkDescriptorSetLayout RayTracer_Generated::CreateCastSingleRayMegaDSLayout()
{
  std::array<VkDescriptorSetLayoutBinding, 2+1+22> dsBindings;

  // binding for out_color
  dsBindings[0].binding            = 0;
  dsBindings[0].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  dsBindings[0].descriptorCount    = 1;
  dsBindings[0].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
  dsBindings[0].pImmutableSamplers = nullptr;

  // binding for m_pAccelStruct
  dsBindings[1].binding            = 1;
  dsBindings[1].descriptorType     = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
  dsBindings[1].descriptorCount    = 1;
  dsBindings[1].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
  dsBindings[1].pImmutableSamplers = nullptr;

  // binding for POD members stored in m_classDataBuffer
  dsBindings[2].binding            = 2;
  dsBindings[2].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  dsBindings[2].descriptorCount    = 1;
  dsBindings[2].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
  dsBindings[2].pImmutableSamplers = nullptr;

  for (int i = 0; i < 6; i++)
  {
    dsBindings[i + 3].binding = i + 3;
    dsBindings[i + 3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    dsBindings[i + 3].descriptorCount = 1;
    dsBindings[i + 3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    dsBindings[i + 3].pImmutableSamplers = nullptr;
  }

  // binding for noise_color
  dsBindings[9].binding            = 9;
  dsBindings[9].descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  dsBindings[9].descriptorCount    = 1;
  dsBindings[9].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
  dsBindings[9].pImmutableSamplers = nullptr;

  // binding for noise_color
  dsBindings[10].binding            = 10;
  dsBindings[10].descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLER;
  dsBindings[10].descriptorCount    = 1;
  dsBindings[10].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
  dsBindings[10].pImmutableSamplers = nullptr;

  // binding for gbufferPos
  dsBindings[11].binding            = 11;
  dsBindings[11].descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  dsBindings[11].descriptorCount    = 1;
  dsBindings[11].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
  dsBindings[11].pImmutableSamplers = nullptr;

  // binding for gbufferNorm
  dsBindings[12].binding            = 12;
  dsBindings[12].descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  dsBindings[12].descriptorCount    = 1;
  dsBindings[12].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
  dsBindings[12].pImmutableSamplers = nullptr;

  // binding for colorSamplerGbuffer
  dsBindings[13].binding            = 13;
  dsBindings[13].descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLER;
  dsBindings[13].descriptorCount    = 1;
  dsBindings[13].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
  dsBindings[13].pImmutableSamplers = nullptr;

  // binding for prevRTImage
  dsBindings[14].binding            = 14;
  dsBindings[14].descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  dsBindings[14].descriptorCount    = 1;
  dsBindings[14].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
  dsBindings[14].pImmutableSamplers = nullptr;

  // binding for prevRTImage sampler
  dsBindings[15].binding            = 15;
  dsBindings[15].descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLER;
  dsBindings[15].descriptorCount    = 1;
  dsBindings[15].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
  dsBindings[15].pImmutableSamplers = nullptr;

  // binding for rtImage static
  dsBindings[16].binding            = 16;
  dsBindings[16].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  dsBindings[16].descriptorCount    = 1;
  dsBindings[16].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
  dsBindings[16].pImmutableSamplers = nullptr;

  // binding for rtImage dynamic
  dsBindings[17].binding            = 17;
  dsBindings[17].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  dsBindings[17].descriptorCount    = 1;
  dsBindings[17].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
  dsBindings[17].pImmutableSamplers = nullptr;

  // binding for depth
  dsBindings[18].binding            = 18;
  dsBindings[18].descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  dsBindings[18].descriptorCount    = 1;
  dsBindings[18].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
  dsBindings[18].pImmutableSamplers = nullptr;

  // binding for prevDepth sampler
  dsBindings[19].binding            = 19;
  dsBindings[19].descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLER;
  dsBindings[19].descriptorCount    = 1;
  dsBindings[19].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
  dsBindings[19].pImmutableSamplers = nullptr;

  // binding for prevDepth
  dsBindings[20].binding            = 20;
  dsBindings[20].descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  dsBindings[20].descriptorCount    = 1;
  dsBindings[20].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
  dsBindings[20].pImmutableSamplers = nullptr;

  // binding for gbufferVelocity
  dsBindings[21].binding            = 21;
  dsBindings[21].descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  dsBindings[21].descriptorCount    = 1;
  dsBindings[21].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
  dsBindings[21].pImmutableSamplers = nullptr;

  // binding for prevNormal
  dsBindings[22].binding            = 22;
  dsBindings[22].descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  dsBindings[22].descriptorCount    = 1;
  dsBindings[22].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
  dsBindings[22].pImmutableSamplers = nullptr;

  // binding for prevСolor sampler
  dsBindings[23].binding            = 23;
  dsBindings[23].descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLER;
  dsBindings[23].descriptorCount    = 1;
  dsBindings[23].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
  dsBindings[23].pImmutableSamplers = nullptr;

  // binding for rtImage AO
  dsBindings[24].binding            = 24;
  dsBindings[24].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  dsBindings[24].descriptorCount    = 1;
  dsBindings[24].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
  dsBindings[24].pImmutableSamplers = nullptr;

  VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
  descriptorSetLayoutCreateInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  descriptorSetLayoutCreateInfo.bindingCount = uint32_t(dsBindings.size());
  descriptorSetLayoutCreateInfo.pBindings    = dsBindings.data();

  VkDescriptorSetLayout layout = nullptr;
  VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, NULL, &layout));
  return layout;
}

VkDescriptorSetLayout RayTracer_Generated::CreatecopyKernelFloatDSLayout()
{
  std::array<VkDescriptorSetLayoutBinding, 2> dsBindings;

  dsBindings[0].binding            = 0;
  dsBindings[0].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  dsBindings[0].descriptorCount    = 1;
  dsBindings[0].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
  dsBindings[0].pImmutableSamplers = nullptr;

  dsBindings[1].binding            = 1;
  dsBindings[1].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  dsBindings[1].descriptorCount    = 1;
  dsBindings[1].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
  dsBindings[1].pImmutableSamplers = nullptr;

  VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
  descriptorSetLayoutCreateInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  descriptorSetLayoutCreateInfo.bindingCount = dsBindings.size();
  descriptorSetLayoutCreateInfo.pBindings    = dsBindings.data();

  VkDescriptorSetLayout layout = nullptr;
  VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, NULL, &layout));
  return layout;
}


void RayTracer_Generated::InitKernel_CastSingleRayMega(const char* a_filePath)
{
  std::string shaderPath = AlterShaderPath("shaders_generated/CastSingleRayMega.comp.spv");

  m_pMaker->LoadShader(device, shaderPath.c_str(), nullptr, "main");
  CastSingleRayMegaDSLayout = CreateCastSingleRayMegaDSLayout();
  CastSingleRayMegaLayout   = m_pMaker->MakeLayout(device, { CastSingleRayMegaDSLayout }, 128); // at least 128 bytes for push constants
  CastSingleRayMegaPipeline = m_pMaker->MakePipeline(device);
}


void RayTracer_Generated::InitKernels(const char* a_filePath)
{
  InitKernel_CastSingleRayMega(a_filePath);
}

void RayTracer_Generated::InitBuffers(size_t a_maxThreadsCount, bool a_tempBuffersOverlay)
{
  m_maxThreadCount = a_maxThreadsCount;
  std::vector<VkBuffer> allBuffers;
  allBuffers.reserve(64);

  struct BufferReqPair
  {
    BufferReqPair() {  }
    BufferReqPair(VkBuffer a_buff, VkDevice a_dev) : buf(a_buff) { vkGetBufferMemoryRequirements(a_dev, a_buff, &req); }
    VkBuffer             buf = VK_NULL_HANDLE;
    VkMemoryRequirements req = {};
  };

  struct LocalBuffers
  {
    std::vector<BufferReqPair> bufs;
    size_t                     size = 0;
    std::vector<VkBuffer>      bufsClean;
  };

  std::vector<LocalBuffers> groups;
  groups.reserve(16);

  LocalBuffers localBuffersCastSingleRay;
  localBuffersCastSingleRay.bufs.reserve(32);
  CastSingleRay_local.rayDirAndFarBuffer = vk_utils::createBuffer(device, sizeof(LiteMath::float4)*a_maxThreadsCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
  localBuffersCastSingleRay.bufs.push_back(BufferReqPair(CastSingleRay_local.rayDirAndFarBuffer, device));
  CastSingleRay_local.rayPosAndNearBuffer = vk_utils::createBuffer(device, sizeof(LiteMath::float4)*a_maxThreadsCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
  localBuffersCastSingleRay.bufs.push_back(BufferReqPair(CastSingleRay_local.rayPosAndNearBuffer, device));
  for(const auto& pair : localBuffersCastSingleRay.bufs)
  {
    allBuffers.push_back(pair.buf);
    localBuffersCastSingleRay.size += pair.req.size;
  }
  groups.push_back(localBuffersCastSingleRay);


  size_t largestIndex = 0;
  size_t largestSize  = 0;
  for(size_t i=0;i<groups.size();i++)
  {
    if(groups[i].size > largestSize)
    {
      largestIndex = i;
      largestSize  = groups[i].size;
    }
    groups[i].bufsClean.resize(groups[i].bufs.size());
    for(size_t j=0;j<groups[i].bufsClean.size();j++)
      groups[i].bufsClean[j] = groups[i].bufs[j].buf;
  }

  auto& allBuffersRef = a_tempBuffersOverlay ? groups[largestIndex].bufsClean : allBuffers;

  m_classDataBuffer = vk_utils::createBuffer(device, sizeof(m_uboData),  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | GetAdditionalFlagsForUBO());
  allBuffersRef.push_back(m_classDataBuffer);

  auto internalBuffersMem = AllocAndBind(allBuffersRef);
  if(a_tempBuffersOverlay)
  {
    for(size_t i=0;i<groups.size();i++)
      if(i != largestIndex)
        AssignBuffersToMemory(groups[i].bufsClean, internalBuffersMem.memObject);
  }
}

void RayTracer_Generated::InitMemberBuffers()
{
  std::vector<VkBuffer> memberVectors;
  std::vector<VkImage>  memberTextures;



  AllocMemoryForMemberBuffersAndImages(memberVectors, memberTextures);
}




void RayTracer_Generated::AssignBuffersToMemory(const std::vector<VkBuffer>& a_buffers, VkDeviceMemory a_mem)
{
  if(a_buffers.size() == 0 || a_mem == VK_NULL_HANDLE)
    return;

  std::vector<VkMemoryRequirements> memInfos(a_buffers.size());
  for(size_t i=0;i<memInfos.size();i++)
  {
    if(a_buffers[i] != VK_NULL_HANDLE)
      vkGetBufferMemoryRequirements(device, a_buffers[i], &memInfos[i]);
    else
    {
      memInfos[i] = memInfos[0];
      memInfos[i].size = 0;
    }
  }

  for(size_t i=1;i<memInfos.size();i++)
  {
    if(memInfos[i].memoryTypeBits != memInfos[0].memoryTypeBits)
    {
      std::cout << "[RayTracer_Generated::AssignBuffersToMemory]: error, input buffers has different 'memReq.memoryTypeBits'" << std::endl;
      return;
    }
  }

  auto offsets = vk_utils::calculateMemOffsets(memInfos);
  for (size_t i = 0; i < memInfos.size(); i++)
  {
    if(a_buffers[i] != VK_NULL_HANDLE)
      vkBindBufferMemory(device, a_buffers[i], a_mem, offsets[i]);
  }
}

RayTracer_Generated::MemLoc RayTracer_Generated::AllocAndBind(const std::vector<VkBuffer>& a_buffers)
{
  MemLoc currLoc;
  if(a_buffers.size() > 0)
  {
    currLoc.memObject = vk_utils::allocateAndBindWithPadding(device, physicalDevice, a_buffers);
    currLoc.allocId   = m_allMems.size();
    m_allMems.push_back(currLoc);
  }
  return currLoc;
}

RayTracer_Generated::MemLoc RayTracer_Generated::AllocAndBind(const std::vector<VkImage>& a_images)
{
  MemLoc currLoc;
  if(a_images.size() > 0)
  {
    std::vector<VkMemoryRequirements> reqs(a_images.size());
    for(size_t i=0; i<reqs.size(); i++)
      vkGetImageMemoryRequirements(device, a_images[i], &reqs[i]);

    for(size_t i=0; i<reqs.size(); i++)
    {
      if(reqs[i].memoryTypeBits != reqs[0].memoryTypeBits)
      {
        std::cout << "RayTracer_Generated::AllocAndBind(textures): memoryTypeBits warning, need to split mem allocation (override me)" << std::endl;
        break;
      }
    }

    auto offsets  = vk_utils::calculateMemOffsets(reqs);
    auto memTotal = offsets[offsets.size() - 1];

    VkMemoryAllocateInfo allocateInfo = {};
    allocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.pNext           = nullptr;
    allocateInfo.allocationSize  = memTotal;
    allocateInfo.memoryTypeIndex = vk_utils::findMemoryType(reqs[0].memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, physicalDevice);
    VK_CHECK_RESULT(vkAllocateMemory(device, &allocateInfo, NULL, &currLoc.memObject));

    for(size_t i=0;i<a_images.size();i++) {
      VK_CHECK_RESULT(vkBindImageMemory(device, a_images[i], currLoc.memObject, offsets[i]));
    }

    currLoc.allocId = m_allMems.size();
    m_allMems.push_back(currLoc);
  }
  return currLoc;
}

void RayTracer_Generated::FreeAllAllocations(std::vector<MemLoc>& a_memLoc)
{
  // in general you may check 'mem.allocId' for unique to be sure you dont free mem twice
  // for default implementation this is not needed
  for(auto mem : a_memLoc)
    vkFreeMemory(device, mem.memObject, nullptr);
  a_memLoc.resize(0);
}

void RayTracer_Generated::AllocMemoryForMemberBuffersAndImages(const std::vector<VkBuffer>& a_buffers, const std::vector<VkImage>& a_images)
{
  AllocAndBind(a_buffers);
}

