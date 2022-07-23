#include "simple_render.h"
#include "../../utils/input_definitions.h"

#include <geom/vk_mesh.h>
#include <vk_pipeline.h>
#include <vk_buffers.h>

void fillWriteDescriptorSetEntry2(VkDescriptorSet set, VkWriteDescriptorSet& writeDS,
  VkDescriptorImageInfo* imageInfo, VkImageView imageView, VkSampler sampler,int binding) {

  //imageInfo->imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  imageInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfo->imageView  = imageView;
  imageInfo->sampler  = sampler;

  writeDS = VkWriteDescriptorSet{};
  writeDS.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writeDS.dstSet = set;
  writeDS.dstBinding = binding;
  writeDS.descriptorCount = 1;
  writeDS.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ;
  writeDS.pBufferInfo = nullptr;
  writeDS.pImageInfo = imageInfo;
  writeDS.pTexelBufferView = nullptr;
}

void fillWriteDescriptorSetEntry(VkDescriptorSet set, VkWriteDescriptorSet& writeDS,
  VkDescriptorBufferInfo* bufferInfo, VkBuffer buffer, int binding) {

  bufferInfo->buffer = buffer;
  bufferInfo->offset = 0;
  bufferInfo->range = VK_WHOLE_SIZE;

  writeDS = VkWriteDescriptorSet{};
  writeDS.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writeDS.dstSet = set;
  writeDS.dstBinding = binding;
  writeDS.descriptorCount = 1;
  writeDS.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writeDS.pBufferInfo = bufferInfo;
  writeDS.pImageInfo = nullptr;
  writeDS.pTexelBufferView = nullptr;
}

void SimpleRender::SetupGbuffer() {
  m_gBuffer.width = m_width;
	m_gBuffer.height = m_height;

  // Color attachments

  // (World space) Positions
  CreateAttachment(
    VK_FORMAT_R16G16B16A16_SFLOAT,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    &m_gBuffer.position);

  // (World space) Normals
  CreateAttachment(
    VK_FORMAT_R16G16B16A16_SFLOAT,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    &m_gBuffer.normal);

  // Albedo (color)
  CreateAttachment(
    VK_FORMAT_R8G8B8A8_UNORM,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    &m_gBuffer.albedo);

  // Depth attachment

  VkImageUsageFlags flags =  VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  CreateAttachment(
    VK_FORMAT_D32_SFLOAT,
    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
    flags,
    &m_gBuffer.depth);

  // Set up separate renderpass with references to the color and depth attachments
  std::array<VkAttachmentDescription, 4> attachmentDescs = {};

  // Init attachment properties
  for (uint32_t i = 0; i < 4; ++i)
  {
    attachmentDescs[i].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescs[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescs[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescs[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescs[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    if (i == 3)
    {
      attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }
    else
    {
      attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
  }

  // Formats
  attachmentDescs[0].format = m_gBuffer.position.format;
  attachmentDescs[1].format = m_gBuffer.normal.format;
  attachmentDescs[2].format = m_gBuffer.albedo.format;
  attachmentDescs[3].format = m_gBuffer.depth.format;

  std::vector<VkAttachmentReference> colorReferences;
  colorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
  colorReferences.push_back({ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
  colorReferences.push_back({ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

  VkAttachmentReference depthReference = {};
  depthReference.attachment = 3;
  depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.pColorAttachments = colorReferences.data();
  subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());
  subpass.pDepthStencilAttachment = &depthReference;

  // Use subpass dependencies for attachment layout transitions
  std::array<VkSubpassDependency, 2> dependencies;

  dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[0].dstSubpass = 0;
  dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
  dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  dependencies[1].srcSubpass = 0;
  dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
  dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  VkRenderPassCreateInfo renderPassInfo = {};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.pAttachments = attachmentDescs.data();
  renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescs.size());
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  renderPassInfo.dependencyCount = 2;
  renderPassInfo.pDependencies = dependencies.data();

  VK_CHECK_RESULT(vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_gBuffer.renderPass));

  std::array<VkImageView,4> attachments;
  attachments[0] = m_gBuffer.position.view;
  attachments[1] = m_gBuffer.normal.view;
  attachments[2] = m_gBuffer.albedo.view;
  attachments[3] = m_gBuffer.depth.view;

  VkFramebufferCreateInfo fbufCreateInfo = {};
  fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  fbufCreateInfo.pNext = NULL;
  fbufCreateInfo.renderPass = m_gBuffer.renderPass;
  fbufCreateInfo.pAttachments = attachments.data();
  fbufCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  fbufCreateInfo.width = m_gBuffer.width;
  fbufCreateInfo.height = m_gBuffer.height;
  fbufCreateInfo.layers = 1;
  VK_CHECK_RESULT(vkCreateFramebuffer(m_device, &fbufCreateInfo, nullptr, &m_gBuffer.frameBuffer));

  // Create sampler to sample from the color attachments
    VkSamplerCreateInfo sampler {};
    sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler.maxAnisotropy = 1.0f;
		sampler.magFilter = VK_FILTER_NEAREST;
		sampler.minFilter = VK_FILTER_NEAREST;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.addressModeV = sampler.addressModeU;
		sampler.addressModeW = sampler.addressModeU;
		sampler.mipLodBias = 0.0f;
		sampler.maxAnisotropy = 1.0f;
		sampler.minLod = 0.0f;
		sampler.maxLod = 1.0f;
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(m_device, &sampler, nullptr, &m_colorSampler));
}

void SimpleRender::CreateAttachment(
  VkFormat format,
  VkImageUsageFlagBits imageUsageType,
  VkImageUsageFlags usage,
  FrameBufferAttachment *attachment)
{
  VkImageAspectFlags aspectMask = 0;
  VkImageLayout imageLayout;

  attachment->format = format;

  if (imageUsageType & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
  {
    aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  }
  if (imageUsageType & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
  {
    aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  }

  assert(aspectMask > 0);

  VkImageCreateInfo image {};
  image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image.imageType = VK_IMAGE_TYPE_2D;
  image.format = format;
  image.extent.width = m_gBuffer.width;
  image.extent.height = m_gBuffer.height;
  image.extent.depth = 1;
  image.mipLevels = 1;
  image.arrayLayers = 1;
  image.samples = VK_SAMPLE_COUNT_1_BIT;
  image.tiling = VK_IMAGE_TILING_OPTIMAL;
  image.usage = usage | VK_IMAGE_USAGE_SAMPLED_BIT;

  VkMemoryAllocateInfo memAlloc {};
  memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  VkMemoryRequirements memReqs;

  VK_CHECK_RESULT(vkCreateImage(m_device, &image, nullptr, &attachment->image));
  vkGetImageMemoryRequirements(m_device, attachment->image, &memReqs);
  memAlloc.allocationSize = memReqs.size;
  // memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  memAlloc.memoryTypeIndex = vk_utils::findMemoryType(memReqs.memoryTypeBits,
                                                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                          m_physicalDevice);
  VK_CHECK_RESULT(vkAllocateMemory(m_device, &memAlloc, nullptr, &attachment->mem));
  VK_CHECK_RESULT(vkBindImageMemory(m_device, attachment->image, attachment->mem, 0));

  VkImageViewCreateInfo imageView {};
  imageView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  imageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
  imageView.format = format;
  imageView.subresourceRange = {};
  imageView.subresourceRange.aspectMask = aspectMask;
  imageView.subresourceRange.baseMipLevel = 0;
  imageView.subresourceRange.levelCount = 1;
  imageView.subresourceRange.baseArrayLayer = 0;
  imageView.subresourceRange.layerCount = 1;
  imageView.image = attachment->image;
  VK_CHECK_RESULT(vkCreateImageView(m_device, &imageView, nullptr, &attachment->view));
}

void RayTracer_GPU::InitDescriptors(std::shared_ptr<SceneManager> sceneManager, vk_utils::VulkanImageMem noiseMapTex, VkSampler noiseTexSampler) 
{
  std::array<VkDescriptorBufferInfo, 6> descriptorBufferInfo;
  std::array<VkDescriptorImageInfo, 1> descriptorImageInfo;
  std::array<VkWriteDescriptorSet, 7> writeDescriptorSet;

  fillWriteDescriptorSetEntry2(m_allGeneratedDS[0], writeDescriptorSet[0], &descriptorImageInfo[0], noiseMapTex.view, noiseTexSampler, 3);
  fillWriteDescriptorSetEntry(m_allGeneratedDS[0], writeDescriptorSet[1], &descriptorBufferInfo[0], sceneManager->GetVertexBuffer(), 4);
  fillWriteDescriptorSetEntry(m_allGeneratedDS[0], writeDescriptorSet[2], &descriptorBufferInfo[1], sceneManager->GetIndexBuffer(), 5);
  fillWriteDescriptorSetEntry(m_allGeneratedDS[0], writeDescriptorSet[3], &descriptorBufferInfo[2], sceneManager->GetMaterialIDsBuffer(), 6);
  fillWriteDescriptorSetEntry(m_allGeneratedDS[0], writeDescriptorSet[4], &descriptorBufferInfo[3], sceneManager->GetMaterialsBuffer(), 7);
  fillWriteDescriptorSetEntry(m_allGeneratedDS[0], writeDescriptorSet[5], &descriptorBufferInfo[4], sceneManager->GetInstanceMatBuffer(), 8);
  fillWriteDescriptorSetEntry(m_allGeneratedDS[0], writeDescriptorSet[6], &descriptorBufferInfo[5], sceneManager->GetMeshInfoBuffer(), 9);

  vkUpdateDescriptorSets(device, uint32_t(writeDescriptorSet.size()), writeDescriptorSet.data(), 0, NULL);
}

SimpleRender::SimpleRender(uint32_t a_width, uint32_t a_height) : m_width(a_width), m_height(a_height)
{
#ifdef NDEBUG
  m_enableValidation = false;
#else
  m_enableValidation = true;
#endif

  m_raytracedImageData.resize(m_width * m_height);
  pushConst2M.screenSize = vec2(a_width, a_height);
}

void SimpleRender::SetupDeviceFeatures()
{
  if(ENABLE_HARDWARE_RT)
  {
    // m_enabledDeviceFeatures.fillModeNonSolid = VK_TRUE;
    m_enabledRayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
    m_enabledRayQueryFeatures.rayQuery = VK_TRUE;
    m_enabledRayQueryFeatures.pNext = nullptr;
    
    m_enabledDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    m_enabledDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;
    m_enabledDeviceAddressFeatures.pNext = &m_enabledRayQueryFeatures;
    
    m_enabledAccelStructFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    m_enabledAccelStructFeatures.accelerationStructure = VK_TRUE;
    m_enabledAccelStructFeatures.pNext = &m_enabledDeviceAddressFeatures;
    
    m_pDeviceFeatures = &m_enabledAccelStructFeatures;
  }
  else
    m_pDeviceFeatures = nullptr;
    
}

void SimpleRender::SetupDeviceExtensions()
{
  m_deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  //Required for printf Debug
  m_deviceExtensions.push_back(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
  
  if(ENABLE_HARDWARE_RT)
  {
    m_deviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    m_deviceExtensions.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
    // Required by VK_KHR_acceleration_structure
    m_deviceExtensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    m_deviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
    m_deviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
    // Required by VK_KHR_ray_tracing_pipeline
    m_deviceExtensions.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
    // Required by VK_KHR_spirv_1_4
    m_deviceExtensions.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
  }
}

void SimpleRender::GetRTFeatures()
{
  m_accelStructFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;

  VkPhysicalDeviceFeatures2 deviceFeatures2{};
  deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  deviceFeatures2.pNext = &m_accelStructFeatures;
  vkGetPhysicalDeviceFeatures2(m_physicalDevice, &deviceFeatures2);
}

void SimpleRender::SetupValidationLayers()
{
  m_validationLayers.push_back("VK_LAYER_KHRONOS_validation");
  m_validationLayers.push_back("VK_LAYER_LUNARG_monitor");
}

void SimpleRender::InitVulkan(const char** a_instanceExtensions, uint32_t a_instanceExtensionsCount, uint32_t a_deviceId)
{
  for(size_t i = 0; i < a_instanceExtensionsCount; ++i)
  {
    m_instanceExtensions.push_back(a_instanceExtensions[i]);
  }
  m_instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

  SetupValidationLayers();
  VK_CHECK_RESULT(volkInitialize());
  CreateInstance();
  volkLoadInstance(m_instance);

  CreateDevice(a_deviceId);
  volkLoadDevice(m_device);

  GetRTFeatures();

  m_commandPool = vk_utils::createCommandPool(m_device, m_queueFamilyIDXs.graphics,
                                              VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

  m_cmdBuffersDrawMain.reserve(m_framesInFlight);
  m_cmdBuffersDrawMain = vk_utils::createCommandBuffers(m_device, m_commandPool, m_framesInFlight);
  m_cmdBuffersGbuffer = vk_utils::createCommandBuffers(m_device, m_commandPool, m_framesInFlight);

  m_frameFences.resize(m_framesInFlight);
  VkFenceCreateInfo fenceInfo = {};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  for (size_t i = 0; i < m_framesInFlight; i++)
  {
    VK_CHECK_RESULT(vkCreateFence(m_device, &fenceInfo, nullptr, &m_frameFences[i]));
  }

  m_pCopyHelper = std::make_shared<vk_utils::PingPongCopyHelper>(m_physicalDevice, m_device, m_transferQueue,
    m_queueFamilyIDXs.transfer, STAGING_MEM_SIZE);

  LoaderConfig conf = {};
  conf.load_geometry = true;
  conf.load_materials = MATERIAL_LOAD_MODE::NONE;
  conf.instance_matrix_as_storage_buffer = true;
  if(ENABLE_HARDWARE_RT)
  {
    conf.build_acc_structs = true;
    conf.build_acc_structs_while_loading_scene = true;
    conf.builder_type = BVH_BUILDER_TYPE::RTX;
  }

  m_pScnMgr = std::make_shared<SceneManager>(m_device, m_physicalDevice, m_queueFamilyIDXs.graphics, m_pCopyHelper, conf);
//  m_pScnMgr = std::make_shared<SceneManager>(m_device, m_physicalDevice, m_queueFamilyIDXs.transfer,
//                                             m_queueFamilyIDXs.graphics, ENABLE_HARDWARE_RT);

}

void SimpleRender::InitPresentation(VkSurfaceKHR &a_surface)
{
  m_surface = a_surface;

  m_presentationResources.queue = m_swapchain.CreateSwapChain(m_physicalDevice, m_device, m_surface,
                                                              m_width, m_height, m_framesInFlight, m_vsync);
  m_presentationResources.currentFrame = 0;

  VkSemaphoreCreateInfo semaphoreInfo = {};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  VK_CHECK_RESULT(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_presentationResources.imageAvailable));
  VK_CHECK_RESULT(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_presentationResources.renderingFinished));
  VK_CHECK_RESULT(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_presentationResources.gbufferFinished));
  m_screenRenderPass = vk_utils::createDefaultRenderPass(m_device, m_swapchain.GetFormat());
  m_quadRenderPass = vk_utils::createDefaultRenderPass(m_device, m_swapchain.GetFormat(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  std::vector<VkFormat> depthFormats = {
      VK_FORMAT_D32_SFLOAT,
      VK_FORMAT_D32_SFLOAT_S8_UINT,
      VK_FORMAT_D24_UNORM_S8_UINT,
      VK_FORMAT_D16_UNORM_S8_UINT,
      VK_FORMAT_D16_UNORM
  };
  vk_utils::getSupportedDepthFormat(m_physicalDevice, depthFormats, &m_depthBuffer.format);
  m_depthBuffer  = vk_utils::createDepthTexture(m_device, m_physicalDevice, m_width, m_height, m_depthBuffer.format);

  SetupGbuffer();

  m_frameBuffers = vk_utils::createFrameBuffers(m_device, m_swapchain, m_screenRenderPass, m_depthBuffer.view);
  m_pGUIRender = std::make_shared<ImGuiRender>(m_instance, m_device, m_physicalDevice, m_queueFamilyIDXs.graphics, m_graphicsQueue, m_swapchain);
}

void SimpleRender::CreateInstance()
{
  VkApplicationInfo appInfo = {};
  appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pNext              = nullptr;
  appInfo.pApplicationName   = "VkRender";
  appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
  appInfo.pEngineName        = "RayTracingSample";
  appInfo.engineVersion      = VK_MAKE_VERSION(0, 1, 0);
  appInfo.apiVersion         = VK_MAKE_VERSION(1, 1, 0);

  m_instance = vk_utils::createInstance(m_enableValidation, m_validationLayers, m_instanceExtensions, &appInfo);

  SetDebugUtilsObjectNameEXT =
      (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(
      m_instance,
      "vkSetDebugUtilsObjectNameEXT");
      
  if (m_enableValidation)
    vk_utils::initDebugReportCallback(m_instance, &debugReportCallbackFn, &m_debugReportCallback);
}

void SimpleRender::CreateDevice(uint32_t a_deviceId)
{
  SetupDeviceExtensions();
  m_physicalDevice = vk_utils::findPhysicalDevice(m_instance, true, a_deviceId, m_deviceExtensions);

  SetupDeviceFeatures();
  m_device = vk_utils::createLogicalDevice(m_physicalDevice, m_validationLayers, m_deviceExtensions,
                                           m_enabledDeviceFeatures, m_queueFamilyIDXs,
                                           VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT, m_pDeviceFeatures);

  vkGetDeviceQueue(m_device, m_queueFamilyIDXs.graphics, 0, &m_graphicsQueue);
  vkGetDeviceQueue(m_device, m_queueFamilyIDXs.transfer, 0, &m_transferQueue);
}

inline VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState(
  VkColorComponentFlags colorWriteMask,
  VkBool32 blendEnable)
{
  VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState {};
  pipelineColorBlendAttachmentState.colorWriteMask = colorWriteMask;
  pipelineColorBlendAttachmentState.blendEnable = blendEnable;
  return pipelineColorBlendAttachmentState;
}

void SimpleRender::SetupSimplePipeline()
{
  m_pBindings->BindBegin(VK_SHADER_STAGE_FRAGMENT_BIT);
  m_pBindings->BindBuffer(0, m_ubo, VK_NULL_HANDLE, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  m_pBindings->BindEnd(&m_dSet, &m_dSetLayout);

  m_pBindings->BindBegin(VK_SHADER_STAGE_FRAGMENT_BIT);
  m_pBindings->BindImage(0, m_gBuffer.position.view, m_colorSampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  m_pBindings->BindImage(1, m_gBuffer.normal.view, m_colorSampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  m_pBindings->BindImage(2, m_gBuffer.albedo.view, m_colorSampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  m_pBindings->BindEnd(&m_dResolveSet, &m_dResolveSetLayout);

  // if we are recreating pipeline (for example, to reload shaders)
  // we need to cleanup old pipeline
  if(m_gBufferPipeline.layout != VK_NULL_HANDLE)
  {
    vkDestroyPipelineLayout(m_device, m_gBufferPipeline.layout, nullptr);
    m_gBufferPipeline.layout = VK_NULL_HANDLE;
  }
  if(m_gBufferPipeline.pipeline != VK_NULL_HANDLE)
  {
    vkDestroyPipeline(m_device, m_gBufferPipeline.pipeline, nullptr);
    m_gBufferPipeline.pipeline = VK_NULL_HANDLE;
  }

  vk_utils::GraphicsPipelineMaker maker;

  std::unordered_map<VkShaderStageFlagBits, std::string> shader_paths;
  shader_paths[VK_SHADER_STAGE_FRAGMENT_BIT] = FRAGMENT_SHADER_PATH + ".spv";
  shader_paths[VK_SHADER_STAGE_VERTEX_BIT]   = VERTEX_SHADER_PATH + ".spv";

  maker.LoadShaders(m_device, shader_paths);

  m_gBufferPipeline.layout = maker.MakeLayout(m_device, {m_dSetLayout}, sizeof(pushConst2M));
  maker.SetDefaultState(m_width, m_height);

  std::array<VkPipelineColorBlendAttachmentState, 3> blendAttachmentStates = {
    pipelineColorBlendAttachmentState(0xf, VK_FALSE),
    pipelineColorBlendAttachmentState(0xf, VK_FALSE),
    pipelineColorBlendAttachmentState(0xf, VK_FALSE)
};

  maker.colorBlending.attachmentCount = static_cast<uint32_t>(blendAttachmentStates.size());
  maker.colorBlending.pAttachments = blendAttachmentStates.data();

  m_gBufferPipeline.pipeline = maker.MakePipeline(m_device, m_pScnMgr->GetPipelineVertexInputStateCreateInfo(),
                                                       m_gBuffer.renderPass, {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});
  
  // make resolve pipeline

  shader_paths[VK_SHADER_STAGE_VERTEX_BIT]   = RESOLVE_VERTEX_SHADER_PATH + ".spv";
  shader_paths[VK_SHADER_STAGE_FRAGMENT_BIT] = RESOLVE_FRAGMENT_SHADER_PATH + ".spv";
  maker.LoadShaders(m_device, shader_paths);
  m_resolvePipeline.layout = maker.MakeLayout(m_device, {m_dResolveSetLayout}, sizeof(pushConst2M));
  maker.SetDefaultState(m_width, m_height);

  VkPipelineColorBlendAttachmentState blend = pipelineColorBlendAttachmentState(0xf, VK_TRUE);
  blend.colorBlendOp = VK_BLEND_OP_ADD;
  blend.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
  blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
  blend.alphaBlendOp = VK_BLEND_OP_ADD;
  blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
  maker.colorBlending.attachmentCount = 1;
  maker.colorBlending.pAttachments = &blend;
  maker.rasterizer.cullMode = VK_CULL_MODE_NONE;
  maker.depthStencilTest.depthTestEnable = true;
  maker.depthStencilTest.depthWriteEnable = false;

  m_resolvePipeline.pipeline = maker.MakePipeline(m_device,  m_pScnMgr->GetPipelineVertexInputStateCreateInfo(),
                                                       m_screenRenderPass, {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});
}

void SimpleRender::SetupTAAPipeline()
{
  m_pBindings->BindBegin(VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT);
  m_pBindings->BindBuffer(0, m_ubo, VK_NULL_HANDLE, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  m_pBindings->BindImage(1, m_rtImage.view, m_rtImageSampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  m_pBindings->BindImage(2, m_taaImage.view, m_taaImageSampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  m_pBindings->BindEnd(&m_quadDS, &m_quadDSLayout);

  // if we are recreating pipeline (for example, to reload shaders)
  // we need to cleanup old pipeline
  // if(m_quadPipeline.layout != VK_NULL_HANDLE)
  // {
  //   vkDestroyPipelineLayout(m_device, m_quadPipeline.layout, nullptr);
  //   m_quadPipeline.layout = VK_NULL_HANDLE;
  // }
  // if(m_quadPipeline.pipeline != VK_NULL_HANDLE)
  // {
  //   vkDestroyPipeline(m_device, m_quadPipeline.pipeline, nullptr);
  //   m_quadPipeline.pipeline = VK_NULL_HANDLE;
  // }

  vk_utils::GraphicsPipelineMaker maker;

  std::unordered_map<VkShaderStageFlagBits, std::string> shader_paths;
  shader_paths[VK_SHADER_STAGE_FRAGMENT_BIT] = TAA_FRAGMENT_SHADER_PATH + ".spv";
  shader_paths[VK_SHADER_STAGE_VERTEX_BIT]   = TAA_VERTEX_SHADER_PATH + ".spv";

  maker.LoadShaders(m_device, shader_paths);

  m_quadPipeline.layout = maker.MakeLayout(m_device, {m_quadDSLayout}, sizeof(pushConst2M));
  maker.SetDefaultState(m_width, m_height);

  m_quadPipeline.pipeline = maker.MakePipeline(m_device, m_pScnMgr->GetPipelineVertexInputStateCreateInfo(),
                                                       m_quadRenderPass, {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});
}

void SimpleRender::CreateUniformBuffer()
{
  VkMemoryRequirements memReq;
  m_ubo = vk_utils::createBuffer(m_device, sizeof(UniformParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &memReq);

  VkMemoryAllocateInfo allocateInfo = {};
  allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocateInfo.pNext = nullptr;
  allocateInfo.allocationSize = memReq.size;
  allocateInfo.memoryTypeIndex = vk_utils::findMemoryType(memReq.memoryTypeBits,
                                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                          m_physicalDevice);
  VK_CHECK_RESULT(vkAllocateMemory(m_device, &allocateInfo, nullptr, &m_uboAlloc));

  VK_CHECK_RESULT(vkBindBufferMemory(m_device, m_ubo, m_uboAlloc, 0));

  vkMapMemory(m_device, m_uboAlloc, 0, sizeof(m_uniforms), 0, &m_uboMappedMem);

  m_uniforms.lightPos  = LiteMath::float4(0.0f, 1.0f,  1.0f, 1.0f);
  m_uniforms.baseColor = LiteMath::float4(0.9f, 0.92f, 1.0f, 0.0f);
  m_uniforms.animateLightColor = true;
  m_uniforms.m_camPos = to_float4(m_cam.pos, 1.0f);
  m_uniforms.m_invProjView = m_inverseProjViewMatrix;

  UpdateUniformBuffer(0.0f);
}

void SimpleRender::UpdateUniformBuffer(float a_time)
{
// most uniforms are updated in GUI -> SetupGUIElements()
  m_uniforms.time = a_time;
  // m_uniforms.m_camPos = to_float4(m_cam.pos, 1.0f);
  // m_uniforms.m_invProjView = m_inverseProjViewMatrix;

  memcpy(m_uboMappedMem, &m_uniforms, sizeof(m_uniforms));
}

void SimpleRender::BuildGbufferCommandBuffer(VkCommandBuffer a_cmdBuff, VkFramebuffer a_frameBuff,
                                            VkImageView, VkPipeline a_pipeline)
{
  vkResetCommandBuffer(a_cmdBuff, 0);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

  VK_CHECK_RESULT(vkBeginCommandBuffer(a_cmdBuff, &beginInfo));

  vk_utils::setDefaultViewport(a_cmdBuff, static_cast<float>(m_width), static_cast<float>(m_height));
  vk_utils::setDefaultScissor(a_cmdBuff, m_width, m_height);

  ///// draw final scene to screen
  {
    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_gBuffer.renderPass;
    renderPassInfo.framebuffer = a_frameBuff;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent.width = m_gBuffer.width;
		renderPassInfo.renderArea.extent.height = m_gBuffer.height;

    VkClearValue clearValues[4] = {};
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[2].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[3].depthStencil = { 1.0f, 0 };
    renderPassInfo.clearValueCount = 4;
    renderPassInfo.pClearValues = &clearValues[0];

    vkCmdBeginRenderPass(a_cmdBuff, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, a_pipeline);

    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gBufferPipeline.layout, 0, 1,
                            &m_dSet, 0, VK_NULL_HANDLE);

    VkShaderStageFlags stageFlags = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    VkDeviceSize zero_offset = 0u;
    VkBuffer vertexBuf = m_pScnMgr->GetVertexBuffer();
    VkBuffer indexBuf = m_pScnMgr->GetIndexBuffer();

    vkCmdBindVertexBuffers(a_cmdBuff, 0, 1, &vertexBuf, &zero_offset);
    vkCmdBindIndexBuffer(a_cmdBuff, indexBuf, 0, VK_INDEX_TYPE_UINT32);

    float4 colors[3] = {
      float4(1.f, 0.f, 0.f, 1.f),
      float4(0.f, 1.f, 0.f, 1.f),
      float4(0.f, 0.f, 1.f, 1.f),
    };

    for (uint32_t i = 0; i < m_pScnMgr->InstancesNum(); ++i)
    {
      auto inst = m_pScnMgr->GetInstanceInfo(i);

      pushConst2M.model = m_pScnMgr->GetInstanceMatrix(i);
      pushConst2M.color = colors[i % 3];
      vkCmdPushConstants(a_cmdBuff, m_gBufferPipeline.layout, stageFlags, 0,
                         sizeof(pushConst2M), &pushConst2M);

      auto mesh_info = m_pScnMgr->GetMeshInfo(inst.mesh_id);
      vkCmdDrawIndexed(a_cmdBuff, mesh_info.m_indNum, 1, mesh_info.m_indexOffset, mesh_info.m_vertexOffset, 0);
    }

    vkCmdEndRenderPass(a_cmdBuff);
  }

  VK_CHECK_RESULT(vkEndCommandBuffer(a_cmdBuff));
}

void SimpleRender::BuildResolveCommandBuffer(VkCommandBuffer a_cmdBuff, VkFramebuffer a_frameBuff,
                                            VkImageView, VkPipeline a_pipeline)
{
  vkResetCommandBuffer(a_cmdBuff, 0);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

  VK_CHECK_RESULT(vkBeginCommandBuffer(a_cmdBuff, &beginInfo));

  vk_utils::setDefaultViewport(a_cmdBuff, static_cast<float>(m_width), static_cast<float>(m_height));
  vk_utils::setDefaultScissor(a_cmdBuff, m_width, m_height);

  ///// draw final scene to screen
  {
    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_screenRenderPass;
    renderPassInfo.framebuffer = a_frameBuff;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent.width = m_gBuffer.width;
		renderPassInfo.renderArea.extent.height = m_gBuffer.height;

    VkClearValue clearValues[2] = {};
    clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = 2;
    renderPassInfo.pClearValues = &clearValues[0];

    vkCmdBeginRenderPass(a_cmdBuff, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, a_pipeline);

    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_resolvePipeline.layout, 0, 1,
                            &m_dResolveSet, 0, VK_NULL_HANDLE);
    
    VkDeviceSize zero_offset = 0u;
    VkBuffer vertexBuf = m_pScnMgr->GetVertexBuffer();
    VkBuffer indexBuf = m_pScnMgr->GetIndexBuffer();

    vkCmdBindVertexBuffers(a_cmdBuff, 0, 1, &vertexBuf, &zero_offset);
    vkCmdBindIndexBuffer(a_cmdBuff, indexBuf, 0, VK_INDEX_TYPE_UINT32);

    VkShaderStageFlags stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;

    for (int i = 0; i < m_pScnMgr->LightInstancesNum(); i++) 
    {
      pushConst2M.model = m_pScnMgr->GetLightInstanceMatrix(i);
      pushConst2M.lightPos =  m_pScnMgr->GetLightInstancePos(i);
      pushConst2M.isOutsideLight = LiteMath::length3(to_float4(m_cam.pos, 0.f) - pushConst2M.lightPos) >= pushConst2M.model[0][0];
      std::cout << pushConst2M.isOutsideLight << std::endl;
      vkCmdPushConstants(a_cmdBuff, m_resolvePipeline.layout, stageFlags, 0,
                         sizeof(pushConst2M), &pushConst2M);
      auto mesh_info = m_pScnMgr->GetMeshInfo(m_pScnMgr->GetLightMeshId());
      vkCmdDrawIndexed(a_cmdBuff, mesh_info.m_indNum, 1, mesh_info.m_indexOffset, mesh_info.m_vertexOffset, 0);
    }

    vkCmdEndRenderPass(a_cmdBuff);
  }

  VK_CHECK_RESULT(vkEndCommandBuffer(a_cmdBuff));
}

void SimpleRender::BuildCommandBufferQuad(VkCommandBuffer a_cmdBuff, VkImageView a_targetImageView)
{
  vkResetCommandBuffer(a_cmdBuff, 0);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

  VK_CHECK_RESULT(vkBeginCommandBuffer(a_cmdBuff, &beginInfo));

  vk_utils::setDefaultViewport(a_cmdBuff, static_cast<float>(m_width), static_cast<float>(m_height));
  vk_utils::setDefaultScissor(a_cmdBuff, m_width, m_height);

    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_quadRenderPass;
    renderPassInfo.framebuffer = m_quadFrameBuffer;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_swapchain.GetExtent();
    VkClearValue clearValues[2] = {};
    clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = 2;
    renderPassInfo.pClearValues = &clearValues[0];

    vkCmdBeginRenderPass(a_cmdBuff, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    {
      vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_quadPipeline.pipeline);
      vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_quadPipeline.layout, 0, 1, &m_quadDS, 0, nullptr);

      //vkCmdDrawIndexed(a_cmdBuff, 4, 1, 0, 0, 0);

      // uint32_t radius = static_cast<uint32_t>(filterRadius);
      // vkCmdPushConstants(a_cmdBuff, m_quadPipeline.layout,VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t), &radius);
      vkCmdDrawIndexed(a_cmdBuff, 4, 1, 0, 0, 0);
    }
    vkCmdEndRenderPass(a_cmdBuff);

    // execute copy
    //
    {
      VkImageSubresourceLayers subresourceLayers = {};
      subresourceLayers.aspectMask               = VK_IMAGE_ASPECT_COLOR_BIT;
      subresourceLayers.mipLevel                 = 0;
      subresourceLayers.baseArrayLayer           = 0;
      subresourceLayers.layerCount               = 1;

      VkImageCopy copyRegion = {};
      copyRegion.srcSubresource = subresourceLayers;
      copyRegion.srcOffset = VkOffset3D{ 0, 0, 0 };
      copyRegion.dstSubresource = subresourceLayers;
      copyRegion.dstOffset = VkOffset3D{ 0, 0, 0 };
      copyRegion.extent = VkExtent3D{ uint32_t(m_width), uint32_t(m_height), 1 };
  
      vkCmdCopyImage(a_cmdBuff, m_resImage.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_taaImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
    }

    float scaleAndOffset[4] = { 0.5f, 0.5f, -0.5f, +0.5f };
    m_pFSQuad->SetRenderTarget(a_targetImageView);
    m_pFSQuad->DrawCmd(a_cmdBuff, m_finalQuadDS, scaleAndOffset);

  VK_CHECK_RESULT(vkEndCommandBuffer(a_cmdBuff));
}

void SimpleRender::CleanupPipelineAndSwapchain()
{
  if (!m_cmdBuffersDrawMain.empty())
  {
    vkFreeCommandBuffers(m_device, m_commandPool, static_cast<uint32_t>(m_cmdBuffersDrawMain.size()),
                         m_cmdBuffersDrawMain.data());
    m_cmdBuffersDrawMain.clear();
  }

  if (!m_cmdBuffersGbuffer.empty())
  {
    vkFreeCommandBuffers(m_device, m_commandPool, static_cast<uint32_t>(m_cmdBuffersGbuffer.size()),
                         m_cmdBuffersGbuffer.data());
    m_cmdBuffersGbuffer.clear();
  }

  for (size_t i = 0; i < m_frameFences.size(); i++)
  {
    vkDestroyFence(m_device, m_frameFences[i], nullptr);
  }
  m_frameFences.clear();

  vk_utils::deleteImg(m_device, &m_depthBuffer);

  for (size_t i = 0; i < m_frameBuffers.size(); i++)
  {
    vkDestroyFramebuffer(m_device, m_frameBuffers[i], nullptr);
  }
  m_frameBuffers.clear();

  vkDestroyFramebuffer(m_device,m_quadFrameBuffer,nullptr);

  if(m_gBuffer.frameBuffer != VK_NULL_HANDLE)
  {
    vkDestroyFramebuffer(m_device, m_gBuffer.frameBuffer, nullptr);
    m_gBuffer.frameBuffer = VK_NULL_HANDLE;
  }

  if(m_screenRenderPass != VK_NULL_HANDLE)
  {
    vkDestroyRenderPass(m_device, m_screenRenderPass, nullptr);
    m_screenRenderPass = VK_NULL_HANDLE;
  }

  if(m_quadRenderPass != VK_NULL_HANDLE)
  {
    vkDestroyRenderPass(m_device, m_quadRenderPass, nullptr);
    m_quadRenderPass = VK_NULL_HANDLE;
  }

  vk_utils::deleteImg(m_device, &m_NoiseMapTex);
  if (m_NoiseTexSampler != VK_NULL_HANDLE)
  {
    vkDestroySampler(m_device, m_NoiseTexSampler, VK_NULL_HANDLE);
  }
  if (m_NoiseMapTex.mem != VK_NULL_HANDLE)
  {
    vkFreeMemory(m_device, m_NoiseMapTex.mem, nullptr);
    m_NoiseMapTex.mem = VK_NULL_HANDLE;
  }

  if(m_gBuffer.renderPass != VK_NULL_HANDLE)
  {
    vkDestroyRenderPass(m_device, m_gBuffer.renderPass, nullptr);
    m_gBuffer.renderPass = VK_NULL_HANDLE;
  }

  if(m_colorSampler != VK_NULL_HANDLE)
  {
    vkDestroySampler(m_device, m_colorSampler, nullptr);
    m_colorSampler = VK_NULL_HANDLE;
  }

  m_swapchain.Cleanup();
}

void SimpleRender::RecreateSwapChain()
{
  vkDeviceWaitIdle(m_device);

  CleanupPipelineAndSwapchain();
  auto oldImagesNum = m_swapchain.GetImageCount();
  m_presentationResources.queue = m_swapchain.CreateSwapChain(m_physicalDevice, m_device, m_surface, m_width, m_height,
    oldImagesNum, m_vsync);

  std::vector<VkFormat> depthFormats = {
      VK_FORMAT_D32_SFLOAT,
      VK_FORMAT_D32_SFLOAT_S8_UINT,
      VK_FORMAT_D24_UNORM_S8_UINT,
      VK_FORMAT_D16_UNORM_S8_UINT,
      VK_FORMAT_D16_UNORM
  };                                                            
  vk_utils::getSupportedDepthFormat(m_physicalDevice, depthFormats, &m_depthBuffer.format);
  
  m_screenRenderPass = vk_utils::createDefaultRenderPass(m_device, m_swapchain.GetFormat());
  m_depthBuffer      = vk_utils::createDepthTexture(m_device, m_physicalDevice, m_width, m_height, m_depthBuffer.format);
  m_frameBuffers     = vk_utils::createFrameBuffers(m_device, m_swapchain, m_screenRenderPass, m_depthBuffer.view);

  m_frameFences.resize(m_framesInFlight);
  VkFenceCreateInfo fenceInfo = {};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  for (size_t i = 0; i < m_framesInFlight; i++)
  {
    VK_CHECK_RESULT(vkCreateFence(m_device, &fenceInfo, nullptr, &m_frameFences[i]));
  }

  m_cmdBuffersDrawMain = vk_utils::createCommandBuffers(m_device, m_commandPool, m_framesInFlight);
  for (uint32_t i = 0; i < m_swapchain.GetImageCount(); ++i)
  {
    BuildGbufferCommandBuffer(m_cmdBuffersGbuffer[i], m_gBuffer.frameBuffer, m_swapchain.GetAttachment(i).view,
                           m_gBufferPipeline.pipeline);
    BuildResolveCommandBuffer(m_cmdBuffersDrawMain[i], m_frameBuffers[i], m_swapchain.GetAttachment(i).view,
                           m_resolvePipeline.pipeline);
  }

  // *** ray tracing resources
  m_raytracedImageData.resize(m_width * m_height);
  m_pRayTracerCPU = nullptr;
  m_pRayTracerGPU = nullptr;
  SetupRTImage();
  //SetupTaaImage();
  //SetupQuadRenderer();
  //SetupQuadDescriptors();
  //

  m_pGUIRender->OnSwapchainChanged(m_swapchain);
}

void SimpleRender::ProcessInput(const AppInput &input)
{
  // add keyboard controls here
  // camera movement is processed separately

  // recreate pipeline to reload shaders
  if(input.keyPressed[GLFW_KEY_B])
  {
#ifdef WIN32
    std::system("cd ../resources/shaders && py compile_simple_render_shaders.py");
    std::system("cd ../resources/shaders && py compile_quad_render_shaders.py");
#else
    std::system("cd ../resources/shaders && python3 compile_simple_render_shaders.py");
    std::system("cd ../resources/shaders && python3 compile_quad_render_shaders.py");
#endif

    SetupSimplePipeline();
    //SetupTAAPipeline();
    //SetupQuadRenderer();


    for (uint32_t i = 0; i < m_framesInFlight; ++i)
    {
      BuildGbufferCommandBuffer(m_cmdBuffersGbuffer[i], m_gBuffer.frameBuffer, m_swapchain.GetAttachment(i).view,
                           m_gBufferPipeline.pipeline);
      BuildResolveCommandBuffer(m_cmdBuffersDrawMain[i], m_frameBuffers[i], m_swapchain.GetAttachment(i).view,
                           m_resolvePipeline.pipeline);
    }
  }

  if(input.keyPressed[GLFW_KEY_1])
  {
    m_currentRenderMode = RenderMode::RASTERIZATION;
  }
  else if(input.keyPressed[GLFW_KEY_2])
  {
    m_currentRenderMode = RenderMode::RAYTRACING;
  }

}


void SimpleRender::UpdateCamera(const Camera* cams, uint32_t a_camsCount)
{
  assert(a_camsCount > 0);
  m_cam = cams[0];
  UpdateView();
}

void SimpleRender::UpdateView()
{
  const float aspect   = float(m_width) / float(m_height);
  auto mProjFix        = OpenglToVulkanProjectionMatrixFix();
  m_projectionMatrix   = projectionMatrix(m_cam.fov, aspect, 0.1f, 1000.0f);
  auto mLookAt         = LiteMath::lookAt(m_cam.pos, m_cam.lookAt, m_cam.up);
  auto mWorldViewProj  = mProjFix * m_projectionMatrix * mLookAt;
  pushConst2M.projView = mWorldViewProj;

  m_inverseProjViewMatrix = LiteMath::inverse4x4(m_projectionMatrix * transpose(inverse4x4(mLookAt)));
}

void SimpleRender::LoadScene(const char* path)
{
  m_pScnMgr->LoadScene(path);

  std::vector<float3> lightPos = {
    float3(0.0, 2.0, 1.0), 
    float3(0.0, -1.0, 1.0),
  };

  std::vector<float> lightScale = {5, 2};
  for (int i = 0; i < lightPos.size(); i++) {
     m_pScnMgr->InstanceLight(lightPos[i], lightScale[i]);
  }


  if(ENABLE_HARDWARE_RT)
  {
    m_pScnMgr->BuildAllBLAS();
    m_pScnMgr->BuildTLAS();
  }
  else
  {
    SetupRTScene();
  }

  std::vector<std::pair<VkDescriptorType, uint32_t> > dtypes = {
    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             1},
    {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     3}
  };

  // set large a_maxSets, because every window resize will cause the descriptor set for quad being to be recreated
  m_pBindings = std::make_shared<vk_utils::DescriptorMaker>(m_device, dtypes, 1000);
  
  SetupNoiseImage();
  SetupRTImage();
  //SetupTaaImage();
  
  
  
  CreateUniformBuffer();

  SetupSimplePipeline();
  //SetupTAAPipeline();
  //SetupQuadDescriptors();

//  auto loadedCam = m_pScnMgr->GetCamera(0);
//  m_cam.fov = loadedCam.fov;
//  m_cam.pos = float3(loadedCam.pos);
//  m_cam.up  = float3(loadedCam.up);
//  m_cam.lookAt = float3(loadedCam.lookAt);
//  m_cam.tdist  = loadedCam.farPlane;

  UpdateView();

  for (uint32_t i = 0; i < m_framesInFlight; ++i)
  {
    BuildGbufferCommandBuffer(m_cmdBuffersGbuffer[i], m_gBuffer.frameBuffer, m_swapchain.GetAttachment(i).view,
                           m_gBufferPipeline.pipeline);
    BuildResolveCommandBuffer(m_cmdBuffersDrawMain[i], m_frameBuffers[i], m_swapchain.GetAttachment(i).view,
                           m_resolvePipeline.pipeline);
  }
}

void SimpleRender::DrawFrameSimple(float a_time)
{
  vkWaitForFences(m_device, 1, &m_frameFences[m_presentationResources.currentFrame], VK_TRUE, UINT64_MAX);
  vkResetFences(m_device, 1, &m_frameFences[m_presentationResources.currentFrame]);

  uint32_t imageIdx;
  m_swapchain.AcquireNextImage(m_presentationResources.imageAvailable, &imageIdx);

  auto currentResolveCmdBuf = m_cmdBuffersDrawMain[m_presentationResources.currentFrame];
  auto currentGbufferCmdBuf = m_cmdBuffersGbuffer[m_presentationResources.currentFrame];

  VkSemaphore waitSemaphores[] = {m_presentationResources.imageAvailable};
  VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

  if(m_currentRenderMode == RenderMode::RASTERIZATION)
  {
    BuildGbufferCommandBuffer(currentGbufferCmdBuf, m_gBuffer.frameBuffer, m_swapchain.GetAttachment(imageIdx).view,
    m_gBufferPipeline.pipeline);
  }
  else if(m_currentRenderMode == RenderMode::RAYTRACING)
  {
    if (ENABLE_HARDWARE_RT)
      RayTraceGPU(a_time);
    else
      RayTraceCPU();

    BuildCommandBufferQuad(currentResolveCmdBuf, m_swapchain.GetAttachment(imageIdx).view);
  }

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &currentGbufferCmdBuf;

  VkSemaphore signalSemaphores[] = {m_presentationResources.gbufferFinished};
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  VK_CHECK_RESULT(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, nullptr));

  BuildResolveCommandBuffer(currentResolveCmdBuf, m_frameBuffers[imageIdx], m_swapchain.GetAttachment(imageIdx).view,
                           m_resolvePipeline.pipeline);

  waitSemaphores[0] = m_presentationResources.gbufferFinished;
  submitInfo.pWaitSemaphores = waitSemaphores;
  signalSemaphores[0] = m_presentationResources.renderingFinished;
  submitInfo.pSignalSemaphores = signalSemaphores;
  submitInfo.pCommandBuffers = &currentResolveCmdBuf;

  VK_CHECK_RESULT(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_frameFences[m_presentationResources.currentFrame]));

  VkResult presentRes = m_swapchain.QueuePresent(m_presentationResources.queue, imageIdx,
                                                 m_presentationResources.renderingFinished);

  if (presentRes == VK_ERROR_OUT_OF_DATE_KHR || presentRes == VK_SUBOPTIMAL_KHR)
  {
    RecreateSwapChain();
  }
  else if (presentRes != VK_SUCCESS)
  {
    RUN_TIME_ERROR("Failed to present swapchain image");
  }

  m_presentationResources.currentFrame = (m_presentationResources.currentFrame + 1) % m_framesInFlight;

  vkQueueWaitIdle(m_presentationResources.queue);
}

void SimpleRender::DrawFrame(float a_time, DrawMode a_mode)
{
  UpdateUniformBuffer(a_time);

  switch (a_mode)
  {
  case DrawMode::WITH_GUI:
    SetupGUIElements();
    DrawFrameWithGUI(a_time);
    break;
  case DrawMode::NO_GUI:
    DrawFrameSimple(a_time);
    break;
  default:
    DrawFrameSimple(a_time);
  }
}

void SimpleRender::Cleanup()
{
  m_pGUIRender = nullptr;
  ImGui::DestroyContext();
  CleanupPipelineAndSwapchain();
  if(m_surface != VK_NULL_HANDLE)
  {
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    m_surface = VK_NULL_HANDLE;
  }

  if(m_rtImageSampler != VK_NULL_HANDLE)
  {
    vkDestroySampler(m_device, m_rtImageSampler, nullptr);
    m_rtImageSampler = VK_NULL_HANDLE;
  }
  vk_utils::deleteImg(m_device, &m_rtImage);

  if(m_taaImageSampler != VK_NULL_HANDLE)
  {
    vkDestroySampler(m_device, m_taaImageSampler, nullptr);
    m_taaImageSampler = VK_NULL_HANDLE;
  }
  vk_utils::deleteImg(m_device, &m_taaImage);

  if(m_resImageSampler != VK_NULL_HANDLE)
  {
    vkDestroySampler(m_device, m_resImageSampler, nullptr);
    m_resImageSampler = VK_NULL_HANDLE;
  }
  vk_utils::deleteImg(m_device, &m_resImage);

  m_pFSQuad = nullptr;

  if (m_resolvePipeline.pipeline != VK_NULL_HANDLE)
  {
    vkDestroyPipeline(m_device, m_resolvePipeline.pipeline, nullptr);
    m_resolvePipeline.pipeline = VK_NULL_HANDLE;
  }
  if (m_resolvePipeline.layout != VK_NULL_HANDLE)
  {
    vkDestroyPipelineLayout(m_device, m_resolvePipeline.layout, nullptr);
    m_resolvePipeline.layout = VK_NULL_HANDLE;
  }

  if (m_gBufferPipeline.pipeline != VK_NULL_HANDLE)
  {
    vkDestroyPipeline(m_device, m_gBufferPipeline.pipeline, nullptr);
    m_gBufferPipeline.pipeline = VK_NULL_HANDLE;
  }
  if (m_gBufferPipeline.layout != VK_NULL_HANDLE)
  {
    vkDestroyPipelineLayout(m_device, m_gBufferPipeline.layout, nullptr);
    m_gBufferPipeline.layout = VK_NULL_HANDLE;
  }

  if (m_presentationResources.imageAvailable != VK_NULL_HANDLE)
  {
    vkDestroySemaphore(m_device, m_presentationResources.imageAvailable, nullptr);
    m_presentationResources.imageAvailable = VK_NULL_HANDLE;
  }
  if (m_presentationResources.renderingFinished != VK_NULL_HANDLE)
  {
    vkDestroySemaphore(m_device, m_presentationResources.renderingFinished, nullptr);
    m_presentationResources.renderingFinished = VK_NULL_HANDLE;
  }

  if (m_presentationResources.gbufferFinished != VK_NULL_HANDLE)
  {
    vkDestroySemaphore(m_device, m_presentationResources.gbufferFinished, nullptr);
    m_presentationResources.gbufferFinished = VK_NULL_HANDLE;
  }

  vkDestroyImage(m_device, m_gBuffer.albedo.image, nullptr);
  vkDestroyImage(m_device, m_gBuffer.position.image, nullptr);
  vkDestroyImage(m_device, m_gBuffer.normal.image, nullptr);
  vkDestroyImage(m_device, m_gBuffer.depth.image, nullptr);
  vkDestroyImageView(m_device, m_gBuffer.albedo.view, nullptr);
  vkDestroyImageView(m_device, m_gBuffer.position.view, nullptr);
  vkDestroyImageView(m_device, m_gBuffer.normal.view, nullptr);
  vkDestroyImageView(m_device, m_gBuffer.depth.view, nullptr);
  vkFreeMemory(m_device, m_gBuffer.albedo.mem, nullptr);
  vkFreeMemory(m_device, m_gBuffer.position.mem, nullptr);
  vkFreeMemory(m_device, m_gBuffer.normal.mem, nullptr);
  vkFreeMemory(m_device, m_gBuffer.depth.mem, nullptr);

  if (m_commandPool != VK_NULL_HANDLE)
  {
    vkDestroyCommandPool(m_device, m_commandPool, nullptr);
    m_commandPool = VK_NULL_HANDLE;
  }

  if(m_ubo != VK_NULL_HANDLE)
  {
    vkDestroyBuffer(m_device, m_ubo, nullptr);
    m_ubo = VK_NULL_HANDLE;
  }

  if(m_uboAlloc != VK_NULL_HANDLE)
  {
    vkFreeMemory(m_device, m_uboAlloc, nullptr);
    m_uboAlloc = VK_NULL_HANDLE;
  }

  if(m_genColorBuffer != VK_NULL_HANDLE)
  {
    vkDestroyBuffer(m_device, m_genColorBuffer, nullptr);
    m_genColorBuffer = VK_NULL_HANDLE;
  }

  if(m_colorMem != VK_NULL_HANDLE)
  {
    vkFreeMemory(m_device, m_colorMem, nullptr);
    m_colorMem = VK_NULL_HANDLE;
  }

  m_pRayTracerCPU = nullptr;
  m_pRayTracerGPU = nullptr;

  m_pBindings = nullptr;
  m_pScnMgr   = nullptr;
  m_pCopyHelper = nullptr;

  if(m_device != VK_NULL_HANDLE)
  {
    vkDestroyDevice(m_device, nullptr);
    m_device = VK_NULL_HANDLE;
  }

  if(m_debugReportCallback != VK_NULL_HANDLE)
  {
    vkDestroyDebugReportCallbackEXT(m_instance, m_debugReportCallback, nullptr);
    m_debugReportCallback = VK_NULL_HANDLE;
  }

  if(m_instance != VK_NULL_HANDLE)
  {
    vkDestroyInstance(m_instance, nullptr);
    m_instance = VK_NULL_HANDLE;
  }

  vk_utils::deleteImg(m_device, &m_NoiseMapTex);
  if (m_NoiseTexSampler != VK_NULL_HANDLE)
  {
    vkDestroySampler(m_device, m_NoiseTexSampler, VK_NULL_HANDLE);
  }
  if (m_NoiseMapTex.mem != VK_NULL_HANDLE)
  {
    vkFreeMemory(m_device, m_NoiseMapTex.mem, nullptr);
    m_NoiseMapTex.mem = VK_NULL_HANDLE;
  }
}

/////////////////////////////////

void SimpleRender::SetupGUIElements()
{
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  {
//    ImGui::ShowDemoWindow();
    ImGui::Begin("Your render settings here");
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),"Press '1' for rasterization mode");
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Press '2' for raytracing mode");
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "For change light source see common_generated.h");
    ImGui::NewLine();

    ImGui::ColorEdit3("Meshes base color 1", m_uniforms.baseColor.M, ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoInputs);
    ImGui::SliderFloat3("Light source 1 position", m_uniforms.lightPos.M, -100.f, 100.f);

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImGui::NewLine();
    ImGui::Text("CamPos: x=%.3f, y=%.3f, z= %.3f",GetCurrentCamera().pos.x,GetCurrentCamera().pos.y,GetCurrentCamera().pos.z);
    ImGui::NewLine();
    ImGui::Text("CamDir: x=%.3f, y=%.3f, z= %.3f",GetCurrentCamera().lookAt.x,GetCurrentCamera().lookAt.y,GetCurrentCamera().lookAt.z);
    ImGui::Text("CamDirNorm: x=%.3f, y=%.3f, z= %.3f",GetCurrentCamera().forward().x,GetCurrentCamera().forward().y,GetCurrentCamera().forward().z);
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),"Press 'B' to recompile and reload shaders");
    ImGui::Text("Changing bindings is not supported.");
    ImGui::Text("Vertex shader path: %s", VERTEX_SHADER_PATH.c_str());
    ImGui::Text("Fragment shader path: %s", FRAGMENT_SHADER_PATH.c_str());
    ImGui::End();
  }

  // Rendering
  ImGui::Render();
}

void SimpleRender::DrawFrameWithGUI(float a_time)
{
  vkWaitForFences(m_device, 1, &m_frameFences[m_presentationResources.currentFrame], VK_TRUE, UINT64_MAX);
  vkResetFences(m_device, 1, &m_frameFences[m_presentationResources.currentFrame]);

  uint32_t imageIdx;
  auto result = m_swapchain.AcquireNextImage(m_presentationResources.imageAvailable, &imageIdx);
  if (result == VK_ERROR_OUT_OF_DATE_KHR)
  {
    RecreateSwapChain();
    return;
  }
  else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
  {
    RUN_TIME_ERROR("Failed to acquire the next swapchain image!");
  }

  auto currentResolveCmdBuf = m_cmdBuffersDrawMain[m_presentationResources.currentFrame];
  auto currentGbufferCmdBuf = m_cmdBuffersGbuffer[m_presentationResources.currentFrame];

  VkSemaphore waitSemaphores[] = {m_presentationResources.imageAvailable};
  VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

  if(m_currentRenderMode == RenderMode::RASTERIZATION)
  {
    BuildGbufferCommandBuffer(currentGbufferCmdBuf, m_gBuffer.frameBuffer, m_swapchain.GetAttachment(imageIdx).view,
    m_gBufferPipeline.pipeline);
  }
  else if(m_currentRenderMode == RenderMode::RAYTRACING)
  {
    if (ENABLE_HARDWARE_RT)
      RayTraceGPU(a_time);
    else
      RayTraceCPU();

    BuildCommandBufferQuad(currentGbufferCmdBuf, m_swapchain.GetAttachment(imageIdx).view);
  }

  // ImDrawData* pDrawData = ImGui::GetDrawData();
  // auto currentGUICmdBuf = m_pGUIRender->BuildGUIRenderCommand(imageIdx, pDrawData);

  // std::vector<VkCommandBuffer> submitCmdBufs = { currentGbufferCmdBuf, currentGUICmdBuf};

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &currentGbufferCmdBuf;

  VkSemaphore signalSemaphores[] = {m_presentationResources.gbufferFinished};
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  VK_CHECK_RESULT(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, nullptr));

  BuildResolveCommandBuffer(currentResolveCmdBuf, m_frameBuffers[imageIdx], m_swapchain.GetAttachment(imageIdx).view,
                           m_resolvePipeline.pipeline);

  ImDrawData* pDrawData = ImGui::GetDrawData();
  auto currentGUICmdBuf = m_pGUIRender->BuildGUIRenderCommand(imageIdx, pDrawData);

  std::vector<VkCommandBuffer> submitCmdBufs = { currentResolveCmdBuf, currentGUICmdBuf};

  submitInfo.commandBufferCount = (uint32_t)submitCmdBufs.size();
  submitInfo.pCommandBuffers = submitCmdBufs.data();

  waitSemaphores[0] = m_presentationResources.gbufferFinished;
  submitInfo.pWaitSemaphores = waitSemaphores;
  signalSemaphores[0] = m_presentationResources.renderingFinished;
  submitInfo.pSignalSemaphores = signalSemaphores;

  VK_CHECK_RESULT(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_frameFences[m_presentationResources.currentFrame]));

  VkResult presentRes = m_swapchain.QueuePresent(m_presentationResources.queue, imageIdx,
    m_presentationResources.renderingFinished);

  if (presentRes == VK_ERROR_OUT_OF_DATE_KHR || presentRes == VK_SUBOPTIMAL_KHR)
  {
    RecreateSwapChain();
  }
  else if (presentRes != VK_SUCCESS)
  {
    RUN_TIME_ERROR("Failed to present swapchain image");
  }

  m_presentationResources.currentFrame = (m_presentationResources.currentFrame + 1) % m_framesInFlight;

  vkQueueWaitIdle(m_presentationResources.queue);
}
