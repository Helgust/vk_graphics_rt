#include <render/VulkanRTX.h>
#include "simple_render.h"
#include "raytracing_generated.h"
#include "stb_image.h"

// ***************************************************************************************************************************
// setup full screen quad to display ray traced image
void SimpleRender::SetupQuadRenderer()
{
  vk_utils::RenderTargetInfo2D rtargetInfo = {};
  rtargetInfo.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  rtargetInfo.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  rtargetInfo.format = m_swapchain.GetFormat();
  rtargetInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  rtargetInfo.size   = m_swapchain.GetExtent();
  m_pFSQuad.reset();
  m_pFSQuad = std::make_shared<vk_utils::QuadRenderer>(0,0, m_width, m_height);
  m_pFSQuad->Create(m_device, "../resources/shaders/quad3_vert.vert.spv", "../resources/shaders/my_quad.frag.spv", rtargetInfo);
}

void SimpleRender::SetupQuadDescriptors()
{
  m_pBindings->BindBegin(VK_SHADER_STAGE_FRAGMENT_BIT);
  m_pBindings->BindImage(0, m_resImage.view, m_taaImageSampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  m_pBindings->BindEnd(&m_finalQuadDS, &m_finalQuadDSLayout);
}

struct Pixel
{
  unsigned char r, g, b;
};

void WriteBMP(const char *fname, Pixel *a_pixelData, int width, int height)
{
  int paddedsize = (width * height) * sizeof(Pixel);

  unsigned char bmpfileheader[14] = {'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0};
  unsigned char bmpinfoheader[40] = {40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 24, 0};

  bmpfileheader[2] = (unsigned char)(paddedsize);
  bmpfileheader[3] = (unsigned char)(paddedsize >> 8);
  bmpfileheader[4] = (unsigned char)(paddedsize >> 16);
  bmpfileheader[5] = (unsigned char)(paddedsize >> 24);

  bmpinfoheader[4] = (unsigned char)(width);
  bmpinfoheader[5] = (unsigned char)(width >> 8);
  bmpinfoheader[6] = (unsigned char)(width >> 16);
  bmpinfoheader[7] = (unsigned char)(width >> 24);
  bmpinfoheader[8] = (unsigned char)(height);
  bmpinfoheader[9] = (unsigned char)(height >> 8);
  bmpinfoheader[10] = (unsigned char)(height >> 16);
  bmpinfoheader[11] = (unsigned char)(height >> 24);

  std::ofstream out(fname, std::ios::out | std::ios::binary);
  out.write((const char *)bmpfileheader, 14);
  out.write((const char *)bmpinfoheader, 40);
  out.write((const char *)a_pixelData, paddedsize);
  out.flush();
  out.close();
}

void SaveBMP(const char *fname, const unsigned int *pixels, int w, int h)
{
  std::vector<Pixel> pixels2(w * h);

  for (size_t i = 0; i < pixels2.size(); i++)
  {
    Pixel px;
    px.r = (pixels[i] & 0x00FF0000) >> 16;
    px.g = (pixels[i] & 0x0000FF00) >> 8;
    px.b = (pixels[i] & 0x000000FF);
    pixels2[i] = px;
  }

  WriteBMP(fname, &pixels2[0], w, h);
}

unsigned char* loadImageLDR(const char* a_filename, int &w, int &h, int &channels)
{
  unsigned char* pixels = stbi_load(a_filename, &w, &h, &channels, STBI_rgb_alpha);

  if(w <= 0 || h <= 0 || !pixels)
  {
    return nullptr;
  }

  return pixels;
}


void SimpleRender::SetupNoiseImage() 
{
  int w, h, channels;
  auto pixels = loadImageLDR(NOISE_TEX.c_str(), w, h, channels);
  //SaveBMP("jopa.bmp",noisePixels.data(),NoiseMapWidth,NoiseMapHeight);
  vk_utils::deleteImg(m_device, &m_NoiseMapTex);
  if (m_NoiseTexSampler != VK_NULL_HANDLE)
  {
    vkDestroySampler(m_device, m_NoiseTexSampler, VK_NULL_HANDLE);
  }

  int mipLevels     = 1;
  m_NoiseMapTex     = allocateColorTextureFromDataLDR(m_device, m_physicalDevice, pixels, w, h, mipLevels,
           VK_FORMAT_R8G8B8A8_UNORM, m_pScnMgr->GetCopyHelper());
  if (m_NoiseTexSampler == VK_NULL_HANDLE)
  {
    m_NoiseTexSampler = vk_utils::createSampler(m_device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK);
  }  
}

void SimpleRender::SetupRTImage()
{
  vk_utils::deleteImg(m_device, &m_rtImage);  
  // change format and usage according to your implementation of RT
  m_rtImage.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  createImgAllocAndBind(m_device, m_physicalDevice, m_width, m_height, VK_FORMAT_R8G8B8A8_UNORM,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, &m_rtImage);

  if(m_rtImageSampler == VK_NULL_HANDLE)
  {
    m_rtImageSampler = vk_utils::createSampler(m_device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK);
  }
}

void SimpleRender::SetupOmniShadowImage() // it is prepareCubeMap at Sasha Willems
{
  vk_utils::deleteImg(m_device, &m_omniShadowImage);  
  // change format and usage according to your implementation of RT
  m_omniShadowImage.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  createImgAllocAndBind(m_device, m_physicalDevice, m_shadowWidth, m_shadowHeight, VK_FORMAT_R8G8B8A8_UNORM,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, &m_omniShadowImage);
  setObjectName(m_omniShadowImage.image, VK_OBJECT_TYPE_IMAGE, "omnishadow_image");
  if(m_omniShadowImageSampler == VK_NULL_HANDLE)
  {
    m_omniShadowImageSampler = vk_utils::createSampler(m_device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK);
  }
}

void SimpleRender::SetupTaaImage()
{
  vk_utils::deleteImg(m_device, &m_taaImage);

  // change format and usage according to your implementation of RT
  m_taaImage.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  createImgAllocAndBind(m_device, m_physicalDevice, m_width, m_height, VK_FORMAT_R8G8B8A8_UNORM,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, &m_taaImage);
  setObjectName(m_taaImage.image,VK_OBJECT_TYPE_IMAGE,"taa_image");
  if(m_taaImageSampler == VK_NULL_HANDLE)
  {
    m_taaImageSampler = vk_utils::createSampler(m_device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK);
  }
}
// ***************************************************************************************************************************

// convert geometry data and pass it to acceleration structure builder
void SimpleRender::SetupRTScene()
{
  m_pAccelStruct = std::shared_ptr<ISceneObject>(CreateSceneRT(""));
  m_pAccelStruct->ClearGeom();

  auto meshesData = m_pScnMgr->GetMeshData();
  std::unordered_map<uint32_t, uint32_t> meshMap;
  for(size_t i = 0; i < m_pScnMgr->MeshesNum(); ++i)
  {
    const auto& info = m_pScnMgr->GetMeshInfo(i);
    auto vertices = reinterpret_cast<float*>((char*)meshesData->VertexData() + info.m_vertexOffset * meshesData->SingleVertexSize());
    auto indices = meshesData->IndexData() + info.m_indexOffset;

    auto stride = meshesData->SingleVertexSize() / sizeof(float);
    std::vector<float4> m_vPos4f(info.m_vertNum);
    std::vector<uint32_t> m_indicesReordered(info.m_indNum);
    for(size_t v = 0; v < info.m_vertNum; ++v)
    {
      m_vPos4f[v] = float4(vertices[v * stride + 0], vertices[v * stride + 1], vertices[v * stride + 2], 1.0f);
    }
    memcpy(m_indicesReordered.data(), indices, info.m_indNum * sizeof(m_indicesReordered[0]));

    auto geomId = m_pAccelStruct->AddGeom_Triangles4f(m_vPos4f.data(), m_vPos4f.size(), m_indicesReordered.data(), m_indicesReordered.size());
    meshMap[i] = geomId;
  }

  m_pAccelStruct->ClearScene();
  for(size_t i = 0; i < m_pScnMgr->InstancesNum(); ++i)
  {
    const auto& info = m_pScnMgr->GetInstanceInfo(i);
    if(meshMap.count(info.mesh_id))
      m_pAccelStruct->AddInstance(meshMap[info.mesh_id], m_pScnMgr->GetInstanceMatrix(info.inst_id));
  }
  m_pAccelStruct->CommitScene();
}

// perform ray tracing on the CPU and upload resulting image on the GPU
void SimpleRender::RayTraceCPU()
{
  if(!m_pRayTracerCPU)
  {
    m_pRayTracerCPU = std::make_unique<RayTracer>(m_width, m_height);
    m_pRayTracerCPU->SetScene(m_pAccelStruct);
  }

  m_pRayTracerCPU->UpdateView(m_cam.pos, m_inverseProjViewMatrix);
#pragma omp parallel for default(none)
  for (int64_t j = 0; j < m_height; ++j)
  {
    for (int64_t i = 0; i < m_width; ++i)
    {
      m_pRayTracerCPU->CastSingleRay(i, j, m_raytracedImageData.data());
    }
  }

  m_pCopyHelper->UpdateImage(m_rtImage.image, m_raytracedImageData.data(), m_width, m_height, 4, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void SimpleRender::RayTraceGPU(float a_time)
{
  if(!m_pRayTracerGPU)
  {
    m_pRayTracerGPU = std::make_unique<RayTracer_GPU>(m_width, m_height);
    m_pRayTracerGPU->InitVulkanObjects(m_device, m_physicalDevice, m_width * m_height);
    m_pRayTracerGPU->InitMemberBuffers();

    const size_t bufferSize1 = m_width * m_height * sizeof(uint32_t);

    m_genColorBuffer = vk_utils::createBuffer(m_device, bufferSize1,  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    m_colorMem       = vk_utils::allocateAndBindWithPadding(m_device, m_physicalDevice, {m_genColorBuffer});

    auto tmp = std::make_shared<VulkanRTX>(m_pScnMgr);
    tmp->CommitScene();

    m_pRayTracerGPU->SetScene(tmp);
    m_pRayTracerGPU->SetVulkanInOutFor_CastSingleRay(m_genColorBuffer, 0);
    m_pRayTracerGPU->InitDescriptors(m_pScnMgr, m_NoiseMapTex, m_NoiseTexSampler);
    //m_pRayTracerGPU->InitDescriptors(m_pScnMgr);
    
    m_pRayTracerGPU->UpdateAll(m_pCopyHelper, a_time);
  }

  m_pRayTracerGPU->UpdateView(m_cam.pos, m_inverseProjViewMatrix);
  m_pRayTracerGPU->UpdatePlainMembers(m_pCopyHelper, a_time);
  
  // do ray tracing
  //
  {
    VkCommandBuffer commandBuffer = vk_utils::createCommandBuffer(m_device, m_commandPool);
    setObjectName(commandBuffer, VK_OBJECT_TYPE_COMMAND_BUFFER, "RaytracingCommandBuffer");

    VkCommandBufferBeginInfo beginCommandBufferInfo = {};
    beginCommandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginCommandBufferInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginCommandBufferInfo);
    m_pRayTracerGPU->CastSingleRayCmd(commandBuffer, m_width, m_height, nullptr);
    
    // prepare buffer and image for copy command
    {
      VkBufferMemoryBarrier transferBuff = {};
      
      transferBuff.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
      transferBuff.pNext               = nullptr;
      transferBuff.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      transferBuff.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      transferBuff.size                = VK_WHOLE_SIZE;
      transferBuff.offset              = 0;
      transferBuff.buffer              = m_genColorBuffer;
      transferBuff.srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
      transferBuff.dstAccessMask       = VK_ACCESS_TRANSFER_READ_BIT;

      VkImageMemoryBarrier transferImage;
      transferImage.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      transferImage.pNext               = nullptr;
      transferImage.srcAccessMask       = 0;
      transferImage.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
      transferImage.oldLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      transferImage.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; 
      transferImage.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      transferImage.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      transferImage.image               = m_rtImage.image;

      transferImage.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      transferImage.subresourceRange.baseMipLevel   = 0;
      transferImage.subresourceRange.baseArrayLayer = 0;
      transferImage.subresourceRange.layerCount     = 1;
      transferImage.subresourceRange.levelCount     = 1;
    
      vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &transferBuff, 1, &transferImage);
    }

    // execute copy
    //
    {
      VkImageSubresourceLayers subresourceLayers = {};
      subresourceLayers.aspectMask               = VK_IMAGE_ASPECT_COLOR_BIT;
      subresourceLayers.mipLevel                 = 0;
      subresourceLayers.baseArrayLayer           = 0;
      subresourceLayers.layerCount               = 1;

      VkBufferImageCopy copyRegion = {};
      copyRegion.bufferOffset      = 0;
      copyRegion.bufferRowLength   = uint32_t(m_width);
      copyRegion.bufferImageHeight = uint32_t(m_height);
      copyRegion.imageExtent       = VkExtent3D{ uint32_t(m_width), uint32_t(m_height), 1 };
      copyRegion.imageOffset       = VkOffset3D{ 0, 0, 0 };
      copyRegion.imageSubresource  = subresourceLayers;
  
      vkCmdCopyBufferToImage(commandBuffer, m_genColorBuffer, m_rtImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
    }
    // get back normal image layout
    {
      VkImageMemoryBarrier transferImage;
      transferImage.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      transferImage.pNext               = nullptr;
      transferImage.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
      transferImage.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
      transferImage.oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      transferImage.newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; 
      transferImage.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      transferImage.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      transferImage.image               = m_rtImage.image;

      transferImage.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      transferImage.subresourceRange.baseMipLevel   = 0;
      transferImage.subresourceRange.baseArrayLayer = 0;
      transferImage.subresourceRange.layerCount     = 1;
      transferImage.subresourceRange.levelCount     = 1;
    
      vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &transferImage);
    }


    vkEndCommandBuffer(commandBuffer);

    vk_utils::executeCommandBufferNow(commandBuffer, m_graphicsQueue, m_device);
  }

}