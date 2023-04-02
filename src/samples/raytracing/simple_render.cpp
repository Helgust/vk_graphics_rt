#include "simple_render.h"
#include "../../utils/input_definitions.h"
#include <render/VulkanRTX.h>

#include <geom/vk_mesh.h>
#include <vk_pipeline.h>
#include <vk_buffers.h>

void fillWriteDescriptorSetEntry(VkDescriptorSet set, VkWriteDescriptorSet& writeDS, 
  VkDescriptorBufferInfo* bufferInfo, VkDescriptorImageInfo* imageInfo, VkBuffer buffer, int binding, bool isRWtexture = false,
   int descriptorCount = 1) {
    if (bufferInfo) {
      bufferInfo->buffer = buffer;
      bufferInfo->offset = 0;
      bufferInfo->range  = VK_WHOLE_SIZE;  
    }


    writeDS = VkWriteDescriptorSet{};
    writeDS.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDS.dstSet = set;
    writeDS.dstBinding = binding;
    writeDS.descriptorCount = descriptorCount;

    if (!imageInfo)
      writeDS.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    else if (imageInfo->sampler)
      writeDS.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    else
      writeDS.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    if (isRWtexture)
      writeDS.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writeDS.pBufferInfo = bufferInfo;
    writeDS.pImageInfo = imageInfo;
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
    &m_gBuffer.position, m_gBuffer.width, m_gBuffer.height);

  // (World space) Normals
  CreateAttachment(
    VK_FORMAT_R16G16B16A16_SFLOAT,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    &m_gBuffer.normal, m_gBuffer.width, m_gBuffer.height);

  // Albedo (color)
  CreateAttachment(
    VK_FORMAT_R8G8B8A8_UNORM,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    &m_gBuffer.albedo, m_gBuffer.width, m_gBuffer.height);

  // Velocity (color)
  CreateAttachment(
    VK_FORMAT_R16G16_SFLOAT,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    &m_gBuffer.velocity, m_gBuffer.width, m_gBuffer.height);

  // Depth attachment

  VkImageUsageFlags flags =  VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  CreateAttachment(
    VK_FORMAT_D32_SFLOAT,
    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
    flags,
    &m_gBuffer.depth, m_gBuffer.width, m_gBuffer.height);

  // Set up separate renderpass with references to the color and depth attachments
  std::array<VkAttachmentDescription, 5> attachmentDescs = {};

  // Init attachment properties
  for (uint32_t i = 0; i < 5; ++i)
  {
    attachmentDescs[i].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescs[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescs[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescs[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescs[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    if (i == 3)
    {
      attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
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
  attachmentDescs[4].format = m_gBuffer.velocity.format;

  std::vector<VkAttachmentReference> colorReferences;
  colorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
  colorReferences.push_back({ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
  colorReferences.push_back({ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
  colorReferences.push_back({ 4, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

  VkAttachmentReference depthReference = {};
  depthReference.attachment = 3;
  depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;

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
  setObjectName(m_gBuffer.renderPass, VK_OBJECT_TYPE_RENDER_PASS, "gbuffer_renderpass");
  std::array<VkImageView,5> attachments;
  attachments[0] = m_gBuffer.position.view;
  attachments[1] = m_gBuffer.normal.view;
  attachments[2] = m_gBuffer.albedo.view;
  attachments[3] = m_gBuffer.depth.view;
  attachments[4] = m_gBuffer.velocity.view;

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
  setObjectName(m_gBuffer.frameBuffer, VK_OBJECT_TYPE_FRAMEBUFFER, "gBuffer_framebuffer");

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
  setObjectName(m_colorSampler, VK_OBJECT_TYPE_SAMPLER, "gBuffer_colorSampler");
}


void SimpleRender::SetupOmniShadow() {
  m_omniShadowBuffer.width = m_shadowWidth;
	m_omniShadowBuffer.height = m_shadowHeight;

  // Color attachments

  // Color atachment
  // CreateAttachment(
  //   VK_FORMAT_R16G16B16A16_SFLOAT,
  //   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
  //   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
  //   &m_omniShadowBuffer.albedo, m_omniShadowBuffer.width, m_omniShadowBuffer.height);

    VkFormat fbColorFormat = VK_FORMAT_R32_SFLOAT;
    m_omniShadowBuffer.albedo.format = fbColorFormat;
    m_omniShadowBuffer.depth.format = VK_FORMAT_D32_SFLOAT;
    // Color attachment
    VkImageCreateInfo imageCreateInfo {};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = fbColorFormat;
    imageCreateInfo.extent.width = m_omniShadowBuffer.width;
    imageCreateInfo.extent.height = m_omniShadowBuffer.height;
    imageCreateInfo.extent.depth = 1;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    // Image of the framebuffer is blit source
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkMemoryAllocateInfo memAllocInfo {};
		memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

    VkImageViewCreateInfo colorImageView {};
		colorImageView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
		colorImageView.format = fbColorFormat;
		colorImageView.flags = 0;
		colorImageView.subresourceRange = {};
		colorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		colorImageView.subresourceRange.baseMipLevel = 0;
		colorImageView.subresourceRange.levelCount = 1;
		colorImageView.subresourceRange.baseArrayLayer = 0;
		colorImageView.subresourceRange.layerCount = 1;

    VkMemoryRequirements memReqs;

		VK_CHECK_RESULT(vkCreateImage(m_device, &imageCreateInfo, nullptr, &m_omniShadowBuffer.albedo.image));
		vkGetImageMemoryRequirements(m_device, m_omniShadowBuffer.albedo.image, &memReqs);
		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_physicalDevice);
		VK_CHECK_RESULT(vkAllocateMemory(m_device, &memAllocInfo, nullptr, &m_omniShadowBuffer.albedo.mem));
		VK_CHECK_RESULT(vkBindImageMemory(m_device, m_omniShadowBuffer.albedo.image, m_omniShadowBuffer.albedo.mem, 0));

		VkCommandBuffer layoutCmd = vk_utils::createCommandBuffer(m_device, m_commandPool);
    setObjectName(layoutCmd, VK_OBJECT_TYPE_COMMAND_BUFFER, "omnishadow_layout_buffer");
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VK_CHECK_RESULT(vkBeginCommandBuffer(layoutCmd, &beginInfo));

		vk_utils::setImageLayout(
			layoutCmd,
			m_omniShadowBuffer.albedo.image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		colorImageView.image = m_omniShadowBuffer.albedo.image;
		VK_CHECK_RESULT(vkCreateImageView(m_device, &colorImageView, nullptr, &m_omniShadowBuffer.albedo.view));


  // Depth attachment

  // VkImageUsageFlags flags =  VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  // CreateAttachment(
  //   VK_FORMAT_D32_SFLOAT,
  //   VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
  //   flags,
  //   &m_omniShadowBuffer.depth, m_omniShadowBuffer.width, m_omniShadowBuffer.height);

  imageCreateInfo.format = VK_FORMAT_D32_SFLOAT;
	imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

  VkImageViewCreateInfo depthStencilView {};
  depthStencilView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
  depthStencilView.format = VK_FORMAT_D32_SFLOAT;
  depthStencilView.flags = 0;
  depthStencilView.subresourceRange = {};
  depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  depthStencilView.subresourceRange.baseMipLevel = 0;
  depthStencilView.subresourceRange.levelCount = 1;
  depthStencilView.subresourceRange.baseArrayLayer = 0;
  depthStencilView.subresourceRange.layerCount = 1;

  VK_CHECK_RESULT(vkCreateImage(m_device, &imageCreateInfo, nullptr, &m_omniShadowBuffer.depth.image));
  vkGetImageMemoryRequirements(m_device, m_omniShadowBuffer.depth.image, &memReqs);
  memAllocInfo.allocationSize = memReqs.size;
  memAllocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_physicalDevice);
  VK_CHECK_RESULT(vkAllocateMemory(m_device, &memAllocInfo, nullptr, &m_omniShadowBuffer.depth.mem));
	VK_CHECK_RESULT(vkBindImageMemory(m_device, m_omniShadowBuffer.depth.image, m_omniShadowBuffer.depth.mem, 0));

  vk_utils::setImageLayout(
    layoutCmd,
    m_omniShadowBuffer.depth.image,
    VK_IMAGE_ASPECT_DEPTH_BIT,
    VK_IMAGE_LAYOUT_UNDEFINED,
    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

  VK_CHECK_RESULT(vkEndCommandBuffer(layoutCmd));
  VkSubmitInfo submitInfo {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &layoutCmd;
  // Create fence to ensure that the command buffer has finished executing
  VkFenceCreateInfo fenceCreateInfo {};
  fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceCreateInfo.flags = 0;
  VkFence fence;
  VK_CHECK_RESULT(vkCreateFence(m_device, &fenceCreateInfo, nullptr, &fence));
  // Submit to the queue
  VK_CHECK_RESULT(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, fence));
  // Wait for the fence to signal that command buffer has finished executing
  VK_CHECK_RESULT(vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX));
  vkDestroyFence(m_device, fence, nullptr);
  vkFreeCommandBuffers(m_device, m_commandPool, 1, &layoutCmd);


  depthStencilView.image = m_omniShadowBuffer.depth.image;
	VK_CHECK_RESULT(vkCreateImageView(m_device, &depthStencilView, nullptr, &m_omniShadowBuffer.depth.view));

  // Set up separate renderpass with references to the color and depth attachments
  std::array<VkAttachmentDescription, 2> attachmentDescs = {};

  // Init attachment properties
  for (uint32_t i = 0; i < 2; ++i)
  {
    attachmentDescs[i].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescs[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescs[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescs[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescs[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    if (i == 1)
    {
      attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }
    else
    {
      attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
  }

  // Formats
  attachmentDescs[0].format = m_omniShadowBuffer.albedo.format;
  attachmentDescs[1].format = m_omniShadowBuffer.depth.format;

  std::vector<VkAttachmentReference> colorReferences;
  colorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

  VkAttachmentReference depthReference = {};
  depthReference.attachment = 1;
  depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.pColorAttachments = colorReferences.data();
  subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());
  subpass.pDepthStencilAttachment = &depthReference;

  VkRenderPassCreateInfo renderPassInfo = {};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.pAttachments = attachmentDescs.data();
  renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescs.size());
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;

  VK_CHECK_RESULT(vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_omniShadowBuffer.renderPass));
  setObjectName(m_omniShadowBuffer.renderPass, VK_OBJECT_TYPE_RENDER_PASS, "omnishadow_renderpass");

  std::array<VkImageView,2> attachments;
  attachments[0] = m_omniShadowBuffer.albedo.view;
  attachments[1] = m_omniShadowBuffer.depth.view;

  VkFramebufferCreateInfo fbufCreateInfo = {};
  fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  fbufCreateInfo.pNext = NULL;
  fbufCreateInfo.renderPass = m_omniShadowBuffer.renderPass;
  fbufCreateInfo.pAttachments = attachments.data();
  fbufCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  fbufCreateInfo.width = m_omniShadowBuffer.width;
  fbufCreateInfo.height = m_omniShadowBuffer.height;
  fbufCreateInfo.layers = 1;
  VK_CHECK_RESULT(vkCreateFramebuffer(m_device, &fbufCreateInfo, nullptr, &m_omniShadowBuffer.frameBuffer));
  setObjectName(m_omniShadowBuffer.frameBuffer, VK_OBJECT_TYPE_FRAMEBUFFER, "omnishadow_framebuffer");

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
  setObjectName(m_colorSampler, VK_OBJECT_TYPE_SAMPLER, "omnishadow_colorsampler");


  setObjectName(m_omniShadowBuffer.albedo.image, VK_OBJECT_TYPE_IMAGE, "omnishadow_color");
  setObjectName(m_omniShadowBuffer.depth.image, VK_OBJECT_TYPE_IMAGE, "omnishadow_depth");
}

void SimpleRender::CreateAttachment(
  VkFormat format,
  VkImageUsageFlagBits imageUsageType,
  VkImageUsageFlags usage,
  FrameBufferAttachment *attachment,
  float width, float height)
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
  image.extent.width = width;
  image.extent.height = height;
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

void RayTracer_GPU::InitDescriptors(std::shared_ptr<SceneManager> sceneManager, 
 vk_utils::VulkanImageMem noiseMapTex, VkSampler noiseTexSampler, 
 FrameBuffer a_gbuffer, VkSampler colorSampler, 
 vk_utils::VulkanImageMem a_prevRT,  VkSampler a_prevRTImageSampler,
 vk_utils::VulkanImageMem a_rtImage,  VkSampler a_RtImageSampler,
 vk_utils::VulkanImageMem a_rtImageDynamic,  VkSampler a_rtImageDynamicSampler,
 vk_utils::VulkanImageMem a_prevDepth,  VkSampler a_prevDepthSampler,
 vk_utils::VulkanImageMem a_prevNormal,  VkSampler a_prevColorSampler) 
{
  std::array<VkDescriptorBufferInfo, 6> descriptorBufferInfo;
  std::vector<VkDescriptorImageInfo>	descriptorImageInfos(10);
  std::vector<VkWriteDescriptorSet> writeDescriptorSet(21);


  descriptorImageInfos[0].sampler = nullptr;
  descriptorImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  descriptorImageInfos[0].imageView = noiseMapTex.view;

  descriptorImageInfos[1].sampler = nullptr;
  descriptorImageInfos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  descriptorImageInfos[1].imageView = a_gbuffer.position.view;

  descriptorImageInfos[2].sampler = nullptr;
  descriptorImageInfos[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  descriptorImageInfos[2].imageView = a_gbuffer.normal.view;

  descriptorImageInfos[3].sampler = nullptr;
  descriptorImageInfos[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  descriptorImageInfos[3].imageView = a_prevRT.view;

  descriptorImageInfos[4].sampler = nullptr;
  descriptorImageInfos[4].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  descriptorImageInfos[4].imageView = a_rtImage.view;

  descriptorImageInfos[5].sampler = nullptr;
  descriptorImageInfos[5].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  descriptorImageInfos[5].imageView = a_rtImageDynamic.view;

  descriptorImageInfos[6].sampler = nullptr;
  descriptorImageInfos[6].imageLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
  descriptorImageInfos[6].imageView = a_gbuffer.depth.view;

  descriptorImageInfos[7].sampler = nullptr;
  descriptorImageInfos[7].imageLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
  descriptorImageInfos[7].imageView = a_prevDepth.view;

  descriptorImageInfos[8].sampler = nullptr;
  descriptorImageInfos[8].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  descriptorImageInfos[8].imageView = a_gbuffer.velocity.view;

  descriptorImageInfos[9].sampler = nullptr;
  descriptorImageInfos[9].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  descriptorImageInfos[9].imageView = a_prevNormal.view;
  

  fillWriteDescriptorSetEntry(m_allGeneratedDS[0], writeDescriptorSet[0], &descriptorBufferInfo[0], nullptr, sceneManager->GetVertexBuffer(), 3);
  fillWriteDescriptorSetEntry(m_allGeneratedDS[0], writeDescriptorSet[1], &descriptorBufferInfo[1], nullptr, sceneManager->GetIndexBuffer(), 4);
  fillWriteDescriptorSetEntry(m_allGeneratedDS[0], writeDescriptorSet[2], &descriptorBufferInfo[2], nullptr, sceneManager->GetMaterialIDsBuffer(), 5);
  fillWriteDescriptorSetEntry(m_allGeneratedDS[0], writeDescriptorSet[3], &descriptorBufferInfo[3], nullptr, sceneManager->GetMaterialsBuffer(), 6);
  fillWriteDescriptorSetEntry(m_allGeneratedDS[0], writeDescriptorSet[4], &descriptorBufferInfo[4], nullptr, sceneManager->GetInstanceMatBuffer(), 7);
  fillWriteDescriptorSetEntry(m_allGeneratedDS[0], writeDescriptorSet[5], &descriptorBufferInfo[5], nullptr, sceneManager->GetMeshInfoBuffer(), 8);

  
  VkDescriptorImageInfo samplerInfo;
  samplerInfo.sampler = noiseTexSampler;
  fillWriteDescriptorSetEntry(m_allGeneratedDS[0], writeDescriptorSet[6], nullptr, &descriptorImageInfos[0], VK_NULL_HANDLE, 9);
  fillWriteDescriptorSetEntry(m_allGeneratedDS[0], writeDescriptorSet[7], nullptr, &samplerInfo ,VK_NULL_HANDLE, 10);
  samplerInfo.sampler = colorSampler;
  fillWriteDescriptorSetEntry(m_allGeneratedDS[0], writeDescriptorSet[8], nullptr, &descriptorImageInfos[1], VK_NULL_HANDLE, 11);
  fillWriteDescriptorSetEntry(m_allGeneratedDS[0], writeDescriptorSet[9], nullptr, &descriptorImageInfos[2], VK_NULL_HANDLE, 12);
  fillWriteDescriptorSetEntry(m_allGeneratedDS[0], writeDescriptorSet[10], nullptr, &samplerInfo ,VK_NULL_HANDLE, 13);
  samplerInfo.sampler = a_prevRTImageSampler;
  fillWriteDescriptorSetEntry(m_allGeneratedDS[0], writeDescriptorSet[11], nullptr, &descriptorImageInfos[3] ,VK_NULL_HANDLE, 14);
  fillWriteDescriptorSetEntry(m_allGeneratedDS[0], writeDescriptorSet[12], nullptr, &samplerInfo ,VK_NULL_HANDLE, 15);
  fillWriteDescriptorSetEntry(m_allGeneratedDS[0], writeDescriptorSet[13], nullptr, &descriptorImageInfos[4] ,VK_NULL_HANDLE, 16, true);
  fillWriteDescriptorSetEntry(m_allGeneratedDS[0], writeDescriptorSet[14], nullptr, &descriptorImageInfos[5] ,VK_NULL_HANDLE, 17, true);
  fillWriteDescriptorSetEntry(m_allGeneratedDS[0], writeDescriptorSet[15], nullptr, &descriptorImageInfos[6] ,VK_NULL_HANDLE, 18);//depth
  samplerInfo.sampler = a_prevDepthSampler;
  fillWriteDescriptorSetEntry(m_allGeneratedDS[0], writeDescriptorSet[16], nullptr, &samplerInfo ,VK_NULL_HANDLE, 19);
  fillWriteDescriptorSetEntry(m_allGeneratedDS[0], writeDescriptorSet[17], nullptr, &descriptorImageInfos[7] ,VK_NULL_HANDLE, 20);
  fillWriteDescriptorSetEntry(m_allGeneratedDS[0], writeDescriptorSet[18], nullptr, &descriptorImageInfos[8] ,VK_NULL_HANDLE, 21);//velocity
  fillWriteDescriptorSetEntry(m_allGeneratedDS[0], writeDescriptorSet[19], nullptr, &descriptorImageInfos[9] ,VK_NULL_HANDLE, 22);//velocity
  samplerInfo.sampler = a_prevColorSampler;
  fillWriteDescriptorSetEntry(m_allGeneratedDS[0], writeDescriptorSet[20], nullptr, &samplerInfo ,VK_NULL_HANDLE, 23);
  

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
  m_deviceExtensions.push_back(VK_EXT_ROBUSTNESS_2_EXTENSION_NAME);
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
  m_cmdBuffersRT = vk_utils::createCommandBuffers(m_device, m_commandPool, m_framesInFlight);

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
  prevLightPos = vec4(0.0f);
  m_surface = a_surface;

  m_presentationResources.queue = m_swapchain.CreateSwapChain(m_physicalDevice, m_device, m_surface,
                                                              m_width, m_height, m_framesInFlight, m_vsync);
  m_presentationResources.currentFrame = 0;

  VkSemaphoreCreateInfo semaphoreInfo = {};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  VK_CHECK_RESULT(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_presentationResources.imageAvailable));
  VK_CHECK_RESULT(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_presentationResources.gbufferFinished));
  VK_CHECK_RESULT(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_presentationResources.renderingFinished));
  VK_CHECK_RESULT(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_presentationResources.rtFinished));
  m_screenRenderPass = vk_utils::createDefaultRenderPass(m_device, m_swapchain.GetFormat());
  setObjectName(m_screenRenderPass,VK_OBJECT_TYPE_RENDER_PASS,"screen_renderpass");
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

  //SetupOmniShadow();
  SetupGbuffer();
  SetupHistoryImages();

  m_frameBuffers = vk_utils::createFrameBuffers(m_device, m_swapchain, m_screenRenderPass, m_depthBuffer.view);
  m_pGUIRender = std::make_shared<ImGuiRender>(m_instance, m_device, m_physicalDevice, m_queueFamilyIDXs.graphics, m_graphicsQueue, m_swapchain);

  // create resolveImage
  //
  m_pResolveImage = std::make_shared<vk_utils::RenderTarget>(m_device, VkExtent2D{1024, 1024});

  vk_utils::AttachmentInfo infoDepth;
  infoDepth.format           = VK_FORMAT_R8G8B8A8_UNORM;
  infoDepth.usage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL | VK_IMAGE_USAGE_SAMPLED_BIT;
  infoDepth.imageSampleCount = VK_SAMPLE_COUNT_1_BIT;
  m_resolveImageId              = m_pResolveImage->CreateAttachment(infoDepth);
  auto memReq                = m_pResolveImage->GetMemoryRequirements()[0]; // we know that we have only one texture
  
  // memory for all shadowmaps (well, if you have them more than 1 ...)
  {
    VkMemoryAllocateInfo allocateInfo = {};
    allocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.pNext           = nullptr;
    allocateInfo.allocationSize  = memReq.size;
    allocateInfo.memoryTypeIndex = vk_utils::findMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_physicalDevice);

    VK_CHECK_RESULT(vkAllocateMemory(m_device, &allocateInfo, NULL, &m_memResolveImage));
  }

  m_pResolveImage->CreateViewAndBindMemory(m_memResolveImage, {0});
  m_pResolveImage->CreateDefaultSampler();
  m_pResolveImage->CreateDefaultRenderPass();
  setObjectName(m_pResolveImage->m_attachments[0].image, VK_OBJECT_TYPE_IMAGE, "resolve_Image");

  // create TaaImage
  //
  m_pTaaImage = std::make_shared<vk_utils::RenderTarget>(m_device, VkExtent2D{1024, 1024});

  vk_utils::AttachmentInfo infoRes;
  infoRes.format           = VK_FORMAT_R8G8B8A8_UNORM;
  infoRes.usage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL | VK_IMAGE_USAGE_SAMPLED_BIT;
  infoRes.imageSampleCount = VK_SAMPLE_COUNT_1_BIT;
  m_resolveImageId              = m_pTaaImage->CreateAttachment(infoRes);
  auto memReqTaa                = m_pTaaImage->GetMemoryRequirements()[0]; // we know that we have only one texture
  
  // memory for all shadowmaps (well, if you have them more than 1 ...)
  {
    VkMemoryAllocateInfo allocateInfo = {};
    allocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.pNext           = nullptr;
    allocateInfo.allocationSize  = memReqTaa.size;
    allocateInfo.memoryTypeIndex = vk_utils::findMemoryType(memReqTaa.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_physicalDevice);

    VK_CHECK_RESULT(vkAllocateMemory(m_device, &allocateInfo, NULL, &m_memTaaImage));
  }

  m_pTaaImage->CreateViewAndBindMemory(m_memTaaImage, {0});
  m_pTaaImage->CreateDefaultSampler();
  m_pTaaImage->CreateDefaultRenderPass();
  setObjectName(m_pTaaImage->m_attachments[0].image, VK_OBJECT_TYPE_IMAGE, "taa_Image");

  // create filteredImage
  //
  m_pFilterImage = std::make_shared<vk_utils::RenderTarget>(m_device, VkExtent2D{1024, 1024});

  vk_utils::AttachmentInfo infoFilter;
  infoFilter.format           = VK_FORMAT_R8G8B8A8_UNORM;
  infoFilter.usage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL | VK_IMAGE_USAGE_SAMPLED_BIT;
  infoFilter.imageSampleCount = VK_SAMPLE_COUNT_1_BIT;
  m_resolveImageId              = m_pFilterImage->CreateAttachment(infoFilter);
  auto memReqFilter               = m_pFilterImage->GetMemoryRequirements()[0]; // we know that we have only one texture
  
  // memory for all shadowmaps (well, if you have them more than 1 ...)
  {
    VkMemoryAllocateInfo allocateInfo = {};
    allocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.pNext           = nullptr;
    allocateInfo.allocationSize  = memReqFilter.size;
    allocateInfo.memoryTypeIndex = vk_utils::findMemoryType(memReqFilter.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_physicalDevice);

    VK_CHECK_RESULT(vkAllocateMemory(m_device, &allocateInfo, NULL, &m_memFilterImage));
  }

  m_pFilterImage->CreateViewAndBindMemory(m_memFilterImage, {0});
  m_pFilterImage->CreateDefaultSampler();
  m_pFilterImage->CreateDefaultRenderPass();
  setObjectName(m_pFilterImage->m_attachments[0].image, VK_OBJECT_TYPE_IMAGE, "filterRt_Image");

  // create softRTshadow
  //
  m_pSoftRTImage = std::make_shared<vk_utils::RenderTarget>(m_device, VkExtent2D{1024, 1024});

  vk_utils::AttachmentInfo infoSoftRt;
  infoSoftRt.format           = VK_FORMAT_R8G8B8A8_UNORM;
  infoSoftRt.usage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL | VK_IMAGE_USAGE_SAMPLED_BIT;
  infoSoftRt.imageSampleCount = VK_SAMPLE_COUNT_1_BIT;
  m_resolveImageId              = m_pSoftRTImage->CreateAttachment(infoSoftRt);
  auto memReqSoftRt               = m_pSoftRTImage->GetMemoryRequirements()[0]; // we know that we have only one texture
  
  // memory for all shadowmaps (well, if you have them more than 1 ...)
  {
    VkMemoryAllocateInfo allocateInfo = {};
    allocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.pNext           = nullptr;
    allocateInfo.allocationSize  = memReqSoftRt.size;
    allocateInfo.memoryTypeIndex = vk_utils::findMemoryType(memReqSoftRt.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_physicalDevice);

    VK_CHECK_RESULT(vkAllocateMemory(m_device, &allocateInfo, NULL, &m_memFilterImage));
  }

  m_pSoftRTImage->CreateViewAndBindMemory(m_memFilterImage, {0});
  m_pSoftRTImage->CreateDefaultSampler();
  m_pSoftRTImage->CreateDefaultRenderPass();
  setObjectName(m_pSoftRTImage->m_attachments[0].image, VK_OBJECT_TYPE_IMAGE, "filterRt_Image");
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

  m_pBindings->BindBegin(VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT);
  m_pBindings->BindBuffer(0, m_ubo, VK_NULL_HANDLE, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  m_pBindings->BindEnd(&m_dOmniShadowSet, &m_dOmniShadowSetLayout);

  m_pBindings->BindBegin(VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT);
  m_pBindings->BindBuffer(0, m_ubo, VK_NULL_HANDLE, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  m_pBindings->BindEnd(&m_dSet, &m_dSetLayout);


  m_pBindings->BindBegin(VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT);
  m_pBindings->BindBuffer(0, m_ubo, VK_NULL_HANDLE, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  m_pBindings->BindImage(1, m_rtImage.view, m_rtImageSampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  m_pBindings->BindImage(2, m_rtImageDynamic.view, m_rtImageDynamicSampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  m_pBindings->BindImage(3, m_prevRTImage.view, m_prevRTImageSampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  m_pBindings->BindImage(4, m_gBuffer.velocity.view, m_colorSampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  m_pBindings->BindEnd(&m_dSoftRTSet, &m_dSoftRTSetLayout);

  auto softRtFrame = m_pSoftRTImage->m_attachments[0];

  m_pBindings->BindBegin(VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT);
  m_pBindings->BindBuffer(0, m_ubo, VK_NULL_HANDLE, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  m_pBindings->BindImage(1, m_gBuffer.position.view, m_colorSampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  m_pBindings->BindImage(2, m_gBuffer.normal.view, m_colorSampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  m_pBindings->BindImage(3, m_gBuffer.albedo.view, m_colorSampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  m_pBindings->BindImage(4, m_gBuffer.depth.view, m_colorSampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL);
  //m_pBindings->BindImage(5, m_omniShadowImage.view, m_omniShadowImageSampler,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  m_pBindings->BindImage(6, m_gBuffer.velocity.view, m_colorSampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  m_pBindings->BindImage(7, softRtFrame.view, m_pSoftRTImage->m_sampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  m_pBindings->BindImage(8, m_rtImageDynamic.view, m_rtImageDynamicSampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  m_pBindings->BindImage(9, m_rtImage.view, m_rtImageSampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  m_pBindings->BindEnd(&m_dResolveSet, &m_dResolveSetLayout);

  auto curentFrame = m_pResolveImage->m_attachments[m_resolveImageId];

  m_pBindings->BindBegin(VK_SHADER_STAGE_FRAGMENT_BIT);
  m_pBindings->BindBuffer(0, m_ubo, VK_NULL_HANDLE, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  m_pBindings->BindImage(1, curentFrame.view, m_pResolveImage->m_sampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  m_pBindings->BindImage(2, m_prevFrameImage.view, m_prevFrameImageSampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  m_pBindings->BindImage(3, m_gBuffer.depth.view, m_colorSampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL);
  m_pBindings->BindImage(4, m_prevDepthImage.view, m_prevDepthImageSampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL);
  m_pBindings->BindImage(5, m_gBuffer.velocity.view, m_colorSampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  m_pBindings->BindEnd(&m_dTAASet, &m_dTAASetLayout);

  auto taaFrame = m_pTaaImage->m_attachments[0];
  
  m_pBindings->BindBegin(VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT);
  m_pBindings->BindImage(0, taaFrame.view, m_pTaaImage->m_sampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  m_pBindings->BindEnd(&m_dFilterSet, &m_dFilterSetLayout);

  m_pBindings->BindBegin(VK_SHADER_STAGE_FRAGMENT_BIT);
  m_pBindings->BindImage(0, taaFrame.view, m_pTaaImage->m_sampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  m_pBindings->BindEnd(&m_dResultSet, &m_dResultSetLayout);

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

  if(m_omniShadowPipeline.layout != VK_NULL_HANDLE)
  {
    vkDestroyPipelineLayout(m_device, m_omniShadowPipeline.layout, nullptr);
    m_omniShadowPipeline.layout = VK_NULL_HANDLE;
  }
  if(m_omniShadowPipeline.pipeline != VK_NULL_HANDLE)
  {
    vkDestroyPipeline(m_device, m_omniShadowPipeline.pipeline, nullptr);
    m_omniShadowPipeline.pipeline = VK_NULL_HANDLE;
  }
  
  if(m_taaPipeline.layout != VK_NULL_HANDLE)
  {
    vkDestroyPipelineLayout(m_device, m_taaPipeline.layout, nullptr);
    m_taaPipeline.layout = VK_NULL_HANDLE;
  }
  if(m_taaPipeline.pipeline != VK_NULL_HANDLE)
  {
    vkDestroyPipeline(m_device, m_taaPipeline.pipeline, nullptr);
    m_taaPipeline.pipeline = VK_NULL_HANDLE;
  }

  vk_utils::GraphicsPipelineMaker maker;

  std::unordered_map<VkShaderStageFlagBits, std::string> shader_paths;
  shader_paths[VK_SHADER_STAGE_FRAGMENT_BIT] = MRT_FRAGMENT_SHADER_PATH + ".spv";
  shader_paths[VK_SHADER_STAGE_VERTEX_BIT]   = VERTEX_SHADER_PATH + ".spv";
  maker.viewport.width  = float(m_width);
  maker.viewport.height = float(m_height);
  maker.scissor.extent  = VkExtent2D{ uint32_t(m_width), uint32_t(m_height) };
  maker.LoadShaders(m_device, shader_paths);
  m_gBufferPipeline.layout = maker.MakeLayout(m_device, {m_dSetLayout}, sizeof(pushConst2M));
  setObjectName(m_gBufferPipeline.layout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "gbuffer_pipeline_layout");
  maker.SetDefaultState(m_width, m_height);

  std::array<VkPipelineColorBlendAttachmentState, 4> blendAttachmentStates = {
    pipelineColorBlendAttachmentState(0xf, VK_FALSE),
    pipelineColorBlendAttachmentState(0xf, VK_FALSE),
    pipelineColorBlendAttachmentState(0xf, VK_FALSE),
    pipelineColorBlendAttachmentState(0xf, VK_FALSE),
};

  maker.colorBlending.attachmentCount = static_cast<uint32_t>(blendAttachmentStates.size());
  maker.colorBlending.pAttachments = blendAttachmentStates.data();

  m_gBufferPipeline.pipeline = maker.MakePipeline(m_device, m_pScnMgr->GetPipelineVertexInputStateCreateInfo(),
                                                       m_gBuffer.renderPass, {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});
  setObjectName(m_gBufferPipeline.pipeline, VK_OBJECT_TYPE_PIPELINE, "gbuffer_pipeline");
  
  //  // make shadow pipeline
  // shader_paths[VK_SHADER_STAGE_FRAGMENT_BIT]   = OMNI_SHADOW_FRAGMENT_SHADER_PATH + ".spv";
  // shader_paths[VK_SHADER_STAGE_VERTEX_BIT]   = OMNI_SHADOW_VERTEX_SHADER_PATH + ".spv";
  // maker.LoadShaders(m_device, shader_paths);
  // maker.viewport.width  = float(m_shadowWidth);
  // maker.viewport.height = float(m_shadowHeight);
  // maker.scissor.extent  = VkExtent2D{ uint32_t(m_shadowWidth), uint32_t(m_shadowHeight) };
  // m_omniShadowPipeline.layout = maker.MakeLayout(m_device, {m_dSetLayout}, sizeof(pushConst2M));
  // setObjectName(m_omniShadowPipeline.layout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "omni_shadow_pipeline_layout");
  // maker.SetDefaultState(m_shadowWidth, m_shadowHeight);

  // m_omniShadowPipeline.pipeline = maker.MakePipeline(m_device, m_pScnMgr->GetPipelineVertexInputStateCreateInfo(),
  //                                           m_omniShadowBuffer.renderPass, {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});
  //setObjectName(m_omniShadowPipeline.pipeline, VK_OBJECT_TYPE_PIPELINE, "omni_shadow_pipeline");

  // make resolve pipeline

  shader_paths[VK_SHADER_STAGE_VERTEX_BIT]   = RESOLVE_VERTEX_SHADER_PATH + ".spv";
  shader_paths[VK_SHADER_STAGE_FRAGMENT_BIT] = RESOLVE_FRAGMENT_SHADER_PATH + ".spv";
  maker.LoadShaders(m_device, shader_paths);
  maker.viewport.width  = float(m_width);
  maker.viewport.height = float(m_height);
  maker.scissor.extent  = VkExtent2D{ uint32_t(m_width), uint32_t(m_height) };
  m_resolvePipeline.layout = maker.MakeLayout(m_device, {m_dResolveSetLayout}, sizeof(pushConst2M));
  setObjectName(m_resolvePipeline.layout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "resolve_pipeline_layout");
  maker.SetDefaultState(m_width, m_height);

  // VkPipelineColorBlendAttachmentState blend = pipelineColorBlendAttachmentState(0xf, VK_TRUE);
  // blend.colorBlendOp = VK_BLEND_OP_ADD;
  // blend.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
  // blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
  // blend.alphaBlendOp = VK_BLEND_OP_ADD;
  // blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  // blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
  // maker.colorBlending.attachmentCount = 1;
  // maker.colorBlending.pAttachments = &blend;
  // maker.rasterizer.cullMode = VK_CULL_MODE_NONE;
  // maker.depthStencilTest.depthTestEnable = true;
  // maker.depthStencilTest.depthWriteEnable = false;

  VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
  vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount = 0;
  vertexInputInfo.vertexAttributeDescriptionCount = 0;
  m_resolvePipeline.pipeline = maker.MakePipeline(m_device,  vertexInputInfo,
                                                       m_pResolveImage->m_renderPass, {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});
  setObjectName(m_resolvePipeline.pipeline, VK_OBJECT_TYPE_PIPELINE, "resolve_pipeline");

  shader_paths[VK_SHADER_STAGE_VERTEX_BIT]   = TAA_VERTEX_SHADER_PATH + ".spv";
  shader_paths[VK_SHADER_STAGE_FRAGMENT_BIT] = TAA_FRAGMENT_SHADER_PATH + ".spv";
  maker.LoadShaders(m_device, shader_paths);
  maker.viewport.width  = float(m_width);
  maker.viewport.height = float(m_height);
  maker.scissor.extent  = VkExtent2D{ uint32_t(m_width), uint32_t(m_height) };
  m_taaPipeline.layout = maker.MakeLayout(m_device, {m_dTAASetLayout}, sizeof(pushConst2M));
  setObjectName(m_taaPipeline.layout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "taa_pipeline_layout");
  maker.SetDefaultState(m_width, m_height);


  m_taaPipeline.pipeline = maker.MakePipeline(m_device, vertexInputInfo,
                                            m_pTaaImage->m_renderPass, {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});
  setObjectName(m_taaPipeline.pipeline, VK_OBJECT_TYPE_PIPELINE, "taa_pipeline"); 

  shader_paths[VK_SHADER_STAGE_VERTEX_BIT]   = SOFT_RT_SHADOWS_VERTEX_SHADER_PATH + ".spv";
  shader_paths[VK_SHADER_STAGE_FRAGMENT_BIT] = SOFT_RT_SHADOWS_FRAGMENT_SHADER_PATH + ".spv";
  maker.LoadShaders(m_device, shader_paths);
  maker.viewport.width  = float(m_width);
  maker.viewport.height = float(m_height);
  maker.scissor.extent  = VkExtent2D{ uint32_t(m_width), uint32_t(m_height) };
  m_softShadowPipeline.layout = maker.MakeLayout(m_device, {m_dSoftRTSetLayout}, sizeof(pushConst2M));
  setObjectName(m_softShadowPipeline.layout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "softRt_pipeline_layout");
  maker.SetDefaultState(m_width, m_height);

  m_softShadowPipeline.pipeline = maker.MakePipeline(m_device, vertexInputInfo,
                                            m_pSoftRTImage->m_renderPass, {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});
  setObjectName(m_softShadowPipeline.pipeline, VK_OBJECT_TYPE_PIPELINE, "softRt_pipeline"); 

  shader_paths[VK_SHADER_STAGE_VERTEX_BIT]   = MEDIAN_VERTEX_SHADER_PATH + ".spv";
  shader_paths[VK_SHADER_STAGE_FRAGMENT_BIT] = SHARP_FRAGMENT_SHADER_PATH + ".spv";
  maker.LoadShaders(m_device, shader_paths);
  maker.viewport.width  = float(m_width);
  maker.viewport.height = float(m_height);
  maker.scissor.extent  = VkExtent2D{ uint32_t(m_width), uint32_t(m_height) };
  m_filterPipeline.layout = maker.MakeLayout(m_device, {m_dFilterSetLayout}, sizeof(pushConst2M));
  setObjectName(m_filterPipeline.layout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "filter_pipeline_layout");
  maker.SetDefaultState(m_width, m_height);


  m_filterPipeline.pipeline = maker.MakePipeline(m_device, vertexInputInfo,
                                            m_pFilterImage->m_renderPass, {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});
  setObjectName(m_filterPipeline.pipeline, VK_OBJECT_TYPE_PIPELINE, "filter_pipeline");   

  shader_paths[VK_SHADER_STAGE_VERTEX_BIT]   = RESULT_VERTEX_SHADER_PATH + ".spv";
  shader_paths[VK_SHADER_STAGE_FRAGMENT_BIT] = RESULT_FRAGMENT_SHADER_PATH + ".spv";
  maker.LoadShaders(m_device, shader_paths);
  maker.viewport.width  = float(m_width);
  maker.viewport.height = float(m_height);
  maker.scissor.extent  = VkExtent2D{ uint32_t(m_width), uint32_t(m_height) };
  m_basicForwardPipeline.layout = maker.MakeLayout(m_device, {m_dResultSetLayout}, sizeof(pushConst2M));
  setObjectName(m_basicForwardPipeline.layout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "result_layout");
  maker.SetDefaultState(m_width, m_height);

  m_basicForwardPipeline.pipeline = maker.MakePipeline(m_device,  vertexInputInfo,
                                                       m_screenRenderPass, {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});
  setObjectName(m_basicForwardPipeline.pipeline, VK_OBJECT_TYPE_PIPELINE, "result_pipeline");                                    
}

void SimpleRender::CreateUniformBuffer()
{
  VkMemoryRequirements memReq;
  m_ubo = vk_utils::createBuffer(m_device, sizeof(UniformParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &memReq);

  setObjectName(m_ubo, VK_OBJECT_TYPE_BUFFER, "UboBufffer");
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

  m_uniforms.lights[0].dir  = LiteMath::float4(0.0f, 1.4f, 1.0f, 1.0f);
  m_uniforms.lights[0].pos  = LiteMath::float4(0.0f, 10.0f, 0.0f, 1.0f);
  m_uniforms.lights[0].color  = LiteMath::float4(1.0f, 1.0f,  1.0f, 1.0f);
  m_uniforms.lights[0].radius_lightDist_dummies  = LiteMath::float4(0.1f, 60.0f,  0.0f, 1.0f);
  // m_uniforms.lights[1].dir  = LiteMath::float4(0.0f, 1.0f, 0.0f, 1.0f);
  // m_uniforms.lights[1].pos  = LiteMath::float4(0.0f, 2.0f,  1.0f, 1.0f);
  // m_uniforms.lights[1].color  = LiteMath::float4(1.0f, 0.0f,  0.0f, 1.0f);
  // m_uniforms.lights[1].radius_lightDist_dummies  = LiteMath::float4(5.0f, 20.0f,  0.0f, 1.0f);
  m_uniforms.baseColor = LiteMath::float4(0.9f, 0.92f, 1.0f, 1.0f);
  currentLightPos = m_uniforms.lights[0].pos;
  // m_uniforms.animateLightColor = true;
  //m_uniforms.m_camPos = to_float4(m_cam.pos, 1.0f);
  //m_uniforms.m_invProjView = m_inverseProjViewMatrix;

  UpdateUniformBuffer(0.0f);
}

void SimpleRender::UpdateUniformBuffer(float a_time)
{
// most uniforms are updated in GUI -> SetupGUIElements()
  m_uniforms.m_time_gbuffer_index = vec4(0, 0, a_time, gbuffer_index);
  m_uniforms.settings = int4(taaFlag ? 1 : 0, softShadow ? 1 : 0, 0, 0);
  //m_pScnMgr->MoveCarX(a_time, teleport);
  //m_pScnMgr->MoveCarY(a_time, teleport);
  //m_pScnMgr->RotCarY(a_time, teleport);
  //m_pScnMgr->RotCarX(a_time, teleport);
  //m_pScnMgr->MoveCarZ(a_time, teleport);
  //m_uniforms.PrevVecMat = m_pScnMgr->GetVehicleInstanceMatrix(0);
  memcpy(m_uboMappedMem, &m_uniforms, sizeof(m_uniforms));
}

void SimpleRender::BuildGbufferCommandBuffer(VkCommandBuffer a_cmdBuff, VkFramebuffer a_frameBuff,
                                            VkImageView, VkPipeline a_pipeline)
{
  setObjectName(a_cmdBuff, VK_OBJECT_TYPE_COMMAND_BUFFER, "gbuffer_inside_command_buffer");
  vkResetCommandBuffer(a_cmdBuff, 0);
  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

  VK_CHECK_RESULT(vkBeginCommandBuffer(a_cmdBuff, &beginInfo));

  vk_utils::setDefaultViewport(a_cmdBuff, static_cast<float>(m_width), static_cast<float>(m_height));
  vk_utils::setDefaultScissor(a_cmdBuff, m_width, m_height);

  //AddCmdsShadowmapPass(a_cmdBuff, m_omniShadowBuffer.frameBuffer);
  //omnishadow pass
  if (onlyOneLoadOfShadows)
    for (uint32_t face = 0; face < 6; face++) {
      UpdateCubeFace(face, a_cmdBuff);
    onlyOneLoadOfShadows = false;
  }
  //UpdateCubeFace(faceIndex, a_cmdBuff);
  //UpdateCubeFace(0,a_cmdBuff);
  ///// draw final scene to screen
  {
    vk_utils::setDefaultViewport(a_cmdBuff, static_cast<float>(m_width), static_cast<float>(m_height));
    vk_utils::setDefaultScissor(a_cmdBuff, m_width, m_height);
    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_gBuffer.renderPass;
    renderPassInfo.framebuffer = a_frameBuff;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent.width = m_gBuffer.width;
		renderPassInfo.renderArea.extent.height = m_gBuffer.height;

    VkClearValue clearValues[5] = {};
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[2].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[3].depthStencil = { 1.0f, 0 };
    clearValues[4].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
    renderPassInfo.clearValueCount = 5;
    renderPassInfo.pClearValues = &clearValues[0];

    vkCmdBeginRenderPass(a_cmdBuff, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gBufferPipeline.pipeline);

    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gBufferPipeline.layout, 0, 1,
                            &m_dSet, 0, VK_NULL_HANDLE);

    VkShaderStageFlags stageFlags = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    VkDeviceSize zero_offset = 0u;
    VkBuffer vertexBuf = m_pScnMgr->GetVertexBuffer();
    setObjectName(vertexBuf, VK_OBJECT_TYPE_BUFFER, "vertex_bufffer_1");
    VkBuffer indexBuf = m_pScnMgr->GetIndexBuffer();
    setObjectName(indexBuf, VK_OBJECT_TYPE_BUFFER, "index_bufffer_1");

    vkCmdBindVertexBuffers(a_cmdBuff, 0, 1, &vertexBuf, &zero_offset);
    vkCmdBindIndexBuffer(a_cmdBuff, indexBuf, 0, VK_INDEX_TYPE_UINT32);

    float4 colors[4] = {
      float4(1.f, 0.f, 0.f, 1.f),
      float4(0.f, 1.f, 0.f, 1.f),
      float4(0.f, 0.f, 1.f, 1.f),
      float4(1.f, 1.f, 1.f, 1.f)
    };

    for (uint32_t i = 0; i < m_pScnMgr->InstancesNum()-1; ++i)
    {
      auto inst = m_pScnMgr->GetInstanceInfo(i);

      pushConst2M.model = m_pScnMgr->GetInstanceMatrix(i);
      pushConst2M.vehiclePos =  m_pScnMgr->GetVehicleInstancePos(0);
      pushConst2M.color = colors[i % 4];
      pushConst2M.dynamicBit = int2(0,0);
      vkCmdPushConstants(a_cmdBuff, m_gBufferPipeline.layout, stageFlags, 0,
                         sizeof(pushConst2M), &pushConst2M);

      auto mesh_info = m_pScnMgr->GetMeshInfo(inst.mesh_id);
      vkCmdDrawIndexed(a_cmdBuff, mesh_info.m_indNum, 1, mesh_info.m_indexOffset, mesh_info.m_vertexOffset, 0);
    }

    //Here dynamic render
    for (uint32_t i = 0; i < m_pScnMgr->DynamicInstancesNum()-1; ++i)
    {
      auto inst = m_pScnMgr->GetDynamicInstanceInfo(i);

      pushConst2M.model = m_pScnMgr->GetDynamicInstanceMatrix(i);
      pushConst2M.vehiclePos =  m_pScnMgr->GetVehicleInstancePos(0);
      pushConst2M.color = float4(1.f, 1.f, 0.f, 1.f);
      pushConst2M.dynamicBit = int2(1,0);
      vkCmdPushConstants(a_cmdBuff, m_gBufferPipeline.layout, stageFlags, 0,
                         sizeof(pushConst2M), &pushConst2M);

      auto mesh_info = m_pScnMgr->GetMeshInfo(inst.mesh_id);
      vkCmdDrawIndexed(a_cmdBuff, mesh_info.m_indNum, 1, mesh_info.m_indexOffset, mesh_info.m_vertexOffset, 0);
    }
    vkCmdEndRenderPass(a_cmdBuff);
  }

  VK_CHECK_RESULT(vkEndCommandBuffer(a_cmdBuff));
}

void SimpleRender::CopyImage(VkCommandBuffer a_cmdBuff, VkImage srcImage, VkImage dstImage, 
  uint32_t srcAspectmask,
  uint32_t dstAspectmask,
  VkImageLayout srcLayout,
  VkImageLayout dstLayout)
{
  // Make sure color writes to the framebuffer are finished before using it as transfer source
		vk_utils::setImageLayout(
			a_cmdBuff,
			srcImage,
			srcAspectmask,
			srcLayout,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

		VkImageSubresourceRange imageSubresourceRange = {};
		imageSubresourceRange.aspectMask = srcAspectmask;
		imageSubresourceRange.baseMipLevel = 0;
		imageSubresourceRange.levelCount = 1;
		imageSubresourceRange.baseArrayLayer = 0;
		imageSubresourceRange.layerCount = 1;

		// Change image layout of one cubemap face to transfer destination
		vk_utils::setImageLayout(
			a_cmdBuff,
			dstImage,
			dstLayout,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			imageSubresourceRange);

		// Copy region for transfer from framebuffer to cube face
		VkImageCopy copyRegion = {};

		copyRegion.srcSubresource.aspectMask = srcAspectmask;
		copyRegion.srcSubresource.baseArrayLayer = 0;
		copyRegion.srcSubresource.mipLevel = 0;
		copyRegion.srcSubresource.layerCount = 1;
		copyRegion.srcOffset = { 0, 0, 0 };

		copyRegion.dstSubresource.aspectMask = dstAspectmask;
		copyRegion.dstSubresource.baseArrayLayer = 0;
		copyRegion.dstSubresource.mipLevel = 0;
		copyRegion.dstSubresource.layerCount = 1;
		copyRegion.dstOffset = { 0, 0, 0 };

		copyRegion.extent.width = m_width;
		copyRegion.extent.height = m_height;
		copyRegion.extent.depth = 1;

		// Put image copy into command buffer
		vkCmdCopyImage(
			a_cmdBuff,
			srcImage,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			dstImage,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&copyRegion);

		// Transform framebuffer color attachment back
		vk_utils::setImageLayout(
			a_cmdBuff,
			srcImage,
			srcAspectmask,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			srcLayout);

		// Change image layout of copied face to shader read
		vk_utils::setImageLayout(
			a_cmdBuff,
			dstImage,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			dstLayout,
			imageSubresourceRange);
}

void SimpleRender::BuildResolveCommandBuffer(VkCommandBuffer a_cmdBuff, VkFramebuffer a_frameBuff,
                                            VkImageView, VkPipeline a_pipeline)
{
  setObjectName(a_cmdBuff, VK_OBJECT_TYPE_COMMAND_BUFFER, "resolve_inside_command_buffer");
  vkResetCommandBuffer(a_cmdBuff, 0);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

  VK_CHECK_RESULT(vkBeginCommandBuffer(a_cmdBuff, &beginInfo));

  vk_utils::setImageLayout(
			a_cmdBuff,
			m_rtImage.image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  
  vk_utils::setImageLayout(
			a_cmdBuff,
			m_rtImageDynamic.image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  vk_utils::setDefaultViewport(a_cmdBuff, static_cast<float>(m_width), static_cast<float>(m_height));
  vk_utils::setDefaultScissor(a_cmdBuff, m_width, m_height);
  //here mix prev and new rtShadow

  VkRenderPassBeginInfo softRTRenderPassInfo = {};
  softRTRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  softRTRenderPassInfo.renderPass = m_pSoftRTImage->m_renderPass;
  softRTRenderPassInfo.framebuffer = m_pSoftRTImage->m_framebuffers[0];
  softRTRenderPassInfo.renderArea.offset = {0, 0};
  softRTRenderPassInfo.renderArea.extent.width = m_width;
  softRTRenderPassInfo.renderArea.extent.height = m_height;

  VkClearValue softRTClearValues[3] = {};
  softRTClearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
  softRTClearValues[1].depthStencil = { 1.0f, 0 };
  softRTRenderPassInfo.clearValueCount = 2;
  softRTRenderPassInfo.pClearValues = &softRTClearValues[0];

  vkCmdBeginRenderPass(a_cmdBuff, &softRTRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
  {
    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_softShadowPipeline.pipeline);

    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_softShadowPipeline.layout, 0, 1,
                            &m_dSoftRTSet, 0, VK_NULL_HANDLE);

    VkShaderStageFlags stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
    vkCmdPushConstants(a_cmdBuff, m_softShadowPipeline.layout, stageFlags, 0, sizeof(pushConst2M), &pushConst2M);
    vkCmdDraw(a_cmdBuff, 4, 1, 0, 0);
  }
  vkCmdEndRenderPass(a_cmdBuff);

  if (!forceHistory)
  {
    // copy softRt to prev 
    SimpleRender::CopyImage(a_cmdBuff, m_pSoftRTImage->m_attachments[0].image, m_prevRTImage.image, 
      VK_IMAGE_ASPECT_COLOR_BIT, 
      VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }
  


  ///// draw final scene to scpecial image
  {
    VkRenderPassBeginInfo renderResolveToImage = {};
    renderResolveToImage.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderResolveToImage.renderPass = m_pResolveImage->m_renderPass;
    renderResolveToImage.framebuffer = m_pResolveImage->m_framebuffers[0];
    renderResolveToImage.renderArea.offset = {0, 0};
    renderResolveToImage.renderArea.extent.width = m_gBuffer.width;
		renderResolveToImage.renderArea.extent.height = m_gBuffer.height;

    VkClearValue clearValues[2] = {};
    clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
    clearValues[1].depthStencil = {1.0f, 0};
    renderResolveToImage.clearValueCount = 2;
    renderResolveToImage.pClearValues = &clearValues[0];
  

    std::vector<VkClearValue> clear =  {clearValues[0], clearValues[1]};
    //VkRenderPassBeginInfo renderResolveToImage = m_pResolveImage->GetRenderPassBeginInfo(0, clear);
  
    vkCmdBeginRenderPass(a_cmdBuff, &renderResolveToImage, VK_SUBPASS_CONTENTS_INLINE);
    {
      vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_resolvePipeline.pipeline);

      vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_resolvePipeline.layout, 0, 1,
                              &m_dResolveSet, 0, VK_NULL_HANDLE);

      VkShaderStageFlags stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
      vkCmdPushConstants(a_cmdBuff, m_resolvePipeline.layout, stageFlags, 0, sizeof(pushConst2M), &pushConst2M);
      vkCmdDraw(a_cmdBuff, 4, 1, 0, 0);
    }
    vkCmdEndRenderPass(a_cmdBuff);

    vk_utils::setImageLayout(
    a_cmdBuff,
    m_prevDepthImage.image,
    VK_IMAGE_ASPECT_DEPTH_BIT,
    VK_IMAGE_LAYOUT_UNDEFINED,
    VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL);

    vk_utils::setImageLayout(
    a_cmdBuff,
    m_prevFrameImage.image,
    VK_IMAGE_ASPECT_COLOR_BIT,
    VK_IMAGE_LAYOUT_UNDEFINED,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // render  TAA to screen
    VkRenderPassBeginInfo taaRenderPassInfo = {};
    taaRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    taaRenderPassInfo.renderPass = m_pTaaImage->m_renderPass;
    taaRenderPassInfo.framebuffer = m_pTaaImage->m_framebuffers[0];
    taaRenderPassInfo.renderArea.offset = {0, 0};
    taaRenderPassInfo.renderArea.extent.width = m_width;
		taaRenderPassInfo.renderArea.extent.height = m_height;

    VkClearValue taaClearValues[3] = {};
		taaClearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
		taaClearValues[1].depthStencil = { 1.0f, 0 };
    taaRenderPassInfo.clearValueCount = 2;
    taaRenderPassInfo.pClearValues = &taaClearValues[0];

    vkCmdBeginRenderPass(a_cmdBuff, &taaRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    {
      vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_taaPipeline.pipeline);

      vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_taaPipeline.layout, 0, 1,
                              &m_dTAASet, 0, VK_NULL_HANDLE);

      VkShaderStageFlags stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
      vkCmdPushConstants(a_cmdBuff, m_taaPipeline.layout, stageFlags, 0, sizeof(pushConst2M), &pushConst2M);
      vkCmdDraw(a_cmdBuff, 4, 1, 0, 0);
    }
    vkCmdEndRenderPass(a_cmdBuff);
  }

  //here put simple median filter

    VkRenderPassBeginInfo filterRenderPassInfo = {};
    filterRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    filterRenderPassInfo.renderPass = m_pFilterImage->m_renderPass;
    filterRenderPassInfo.framebuffer = m_pFilterImage->m_framebuffers[0];
    filterRenderPassInfo.renderArea.offset = {0, 0};
    filterRenderPassInfo.renderArea.extent.width = m_width;
		filterRenderPassInfo.renderArea.extent.height = m_height;

    VkClearValue filterClearValues[3] = {};
		filterClearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
		filterClearValues[1].depthStencil = { 1.0f, 0 };
    filterRenderPassInfo.clearValueCount = 2;
    filterRenderPassInfo.pClearValues = &filterClearValues[0];

    vkCmdBeginRenderPass(a_cmdBuff, &filterRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    {
      vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_filterPipeline.pipeline);

      vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_filterPipeline.layout, 0, 1,
                              &m_dFilterSet, 0, VK_NULL_HANDLE);

      VkShaderStageFlags stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
      vkCmdPushConstants(a_cmdBuff, m_filterPipeline.layout, stageFlags, 0, sizeof(pushConst2M), &pushConst2M);
      vkCmdDraw(a_cmdBuff, 4, 1, 0, 0);
    }
    vkCmdEndRenderPass(a_cmdBuff);

  if (!forceHistory)
  {
    //copy to depth to prev
    SimpleRender::CopyImage(a_cmdBuff, m_gBuffer.depth.image, m_prevDepthImage.image, 
    VK_IMAGE_ASPECT_DEPTH_BIT, 
    VK_IMAGE_ASPECT_DEPTH_BIT,
    VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL,
    VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL);

    //copy to normal to prev
    SimpleRender::CopyImage(a_cmdBuff, m_gBuffer.normal.image, m_prevNormalImage.image, 
      VK_IMAGE_ASPECT_COLOR_BIT, 
      VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    
    // copy frame to prev
    SimpleRender::CopyImage(a_cmdBuff, m_pFilterImage->m_attachments[0].image, m_prevFrameImage.image, 
      VK_IMAGE_ASPECT_COLOR_BIT, 
      VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }
  

  // render to screen
    VkRenderPassBeginInfo screenRenderInfo = {};
    screenRenderInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    screenRenderInfo.renderPass = m_screenRenderPass;
    screenRenderInfo.framebuffer = a_frameBuff;
    screenRenderInfo.renderArea.offset = {0, 0};
    screenRenderInfo.renderArea.extent.width = m_width;
    screenRenderInfo.renderArea.extent.height = m_height;

    VkClearValue clearValues[2] = {};
    clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
    clearValues[1].depthStencil = {1.0f, 0};
    screenRenderInfo.clearValueCount = 2;
    screenRenderInfo.pClearValues = &clearValues[0];

    vkCmdBeginRenderPass(a_cmdBuff, &screenRenderInfo, VK_SUBPASS_CONTENTS_INLINE);
    {
      vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_basicForwardPipeline.pipeline);

      vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_basicForwardPipeline.layout, 0, 1,
                              &m_dResultSet, 0, VK_NULL_HANDLE);

      VkShaderStageFlags stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
      vkCmdDraw(a_cmdBuff, 4, 1, 0, 0);
    }
    vkCmdEndRenderPass(a_cmdBuff);

  VK_CHECK_RESULT(vkEndCommandBuffer(a_cmdBuff));
}

void SimpleRender::UpdateCubeFace(uint32_t faceIndex, VkCommandBuffer a_cmdBuff)
{
  // vk_utils::setDefaultViewport(a_cmdBuff, static_cast<float>(m_width), static_cast<float>(m_height));
  // vk_utils::setDefaultScissor(a_cmdBuff, m_width, m_height);
  // VkCommandBufferBeginInfo beginInfo = {};
  // beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  // beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

  VkClearValue clearValues[2];
  clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
  clearValues[1].depthStencil = { 1.0f, 0 };

  VkRenderPassBeginInfo renderPassInfo = {};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = m_omniShadowBuffer.renderPass;
  renderPassInfo.framebuffer = m_omniShadowBuffer.frameBuffer;
  renderPassInfo.renderArea.extent.width = m_omniShadowBuffer.width;
	renderPassInfo.renderArea.extent.height = m_omniShadowBuffer.height;
  renderPassInfo.clearValueCount = 2;
  renderPassInfo.pClearValues = clearValues;

  // Update view matrix via push constant
  float4x4 viewMatrix =  float4x4(); 
  viewMatrix.set_col(3, m_uniforms.lights[0].pos);
  float light_radius = m_uniforms.lights[0].radius_lightDist_dummies.y;
  //float4x4 mProj = ortoMatrix(-light_radius, +light_radius, -light_radius, +light_radius, 0.0f, 100.0f);
  float4x4 mProj = perspectiveMatrix(90, 1.0f, 1.0f, light_radius);
  float4x4 mProjFix = float4x4();
  //float4x4 mProjFix = OpenglToVulkanProjectionMatrixFix();
  float3 light_pos = float3(m_uniforms.lights[0].pos.x, m_uniforms.lights[0].pos.y, m_uniforms.lights[0].pos.z);
		switch (faceIndex)
		{
		case 0: // POSITIVE_X
      viewMatrix = LiteMath::lookAt(light_pos, light_pos + float3(1.0f, 0.0f, 0.0f), float3(0, -1, 0));
      // viewMatrix = rotate4x4Y(90 * DEG_TO_RAD) * viewMatrix;
      // viewMatrix = rotate4x4X(180 * DEG_TO_RAD) * viewMatrix;
			break;
		case 1:	// NEGATIVE_X
      viewMatrix = LiteMath::lookAt(light_pos, light_pos + float3(-1.0f, 0.0f, 0.0f), float3(0, -1, 0));
			// viewMatrix = rotate4x4Y(-90 * DEG_TO_RAD) * viewMatrix;
      // viewMatrix = rotate4x4X(180 * DEG_TO_RAD) * viewMatrix;
			break;
		case 2:	// POSITIVE_Y
      viewMatrix = LiteMath::lookAt(light_pos, light_pos + float3(0.0f, 1.0f, 0.0f), float3(0, 0, 1));
      //viewMatrix = rotate4x4X(-90 * DEG_TO_RAD) * viewMatrix;
			break;
		case 3:	// NEGATIVE_Y
      viewMatrix = LiteMath::lookAt(light_pos, light_pos + float3(0.0f, -1.0f, 0.0f), float3(0, 0, -1));
			//viewMatrix = rotate4x4X(90 * DEG_TO_RAD) * viewMatrix;
			break;
		case 4:	// POSITIVE_Z
      viewMatrix = LiteMath::lookAt(light_pos, light_pos + float3(0.0f, 0.0f, 1.0f), float3(0, -1, 0));
			//viewMatrix = rotate4x4X(180 * DEG_TO_RAD) * viewMatrix ;
			break;
		case 5:	// NEGATIVE_Z
      viewMatrix = LiteMath::lookAt(light_pos, light_pos + float3(0.0f, 0.0f, -1.0f), float3(0, -1, 0));
			//viewMatrix = rotate4x4Z(180 * DEG_TO_RAD) * viewMatrix;
			break;
		}

    float4x4 m_lightMatrix = mProjFix*mProj*viewMatrix;
    pushConst2M.lightView = m_lightMatrix;
  
    vkCmdBeginRenderPass(a_cmdBuff, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    {
      vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_omniShadowPipeline.pipeline);

      vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_omniShadowPipeline.layout, 0, 1,
                              &m_dSet, 0, VK_NULL_HANDLE);

      VkShaderStageFlags stageFlags = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

      VkDeviceSize zero_offset = 0u;
      VkBuffer vertexBuf = m_pScnMgr->GetVertexBuffer();
      setObjectName(vertexBuf, VK_OBJECT_TYPE_BUFFER, "vertex_bufffer_2");
      VkBuffer indexBuf = m_pScnMgr->GetIndexBuffer();
      setObjectName(indexBuf, VK_OBJECT_TYPE_BUFFER, "index_bufffer_2");

      vkCmdBindVertexBuffers(a_cmdBuff, 0, 1, &vertexBuf, &zero_offset);
      vkCmdBindIndexBuffer(a_cmdBuff, indexBuf, 0, VK_INDEX_TYPE_UINT32);

      for (uint32_t i = 0; i < m_pScnMgr->InstancesNum(); ++i)
      {
        auto inst = m_pScnMgr->GetInstanceInfo(i);

        pushConst2M.model = m_pScnMgr->GetInstanceMatrix(i);
        vkCmdPushConstants(a_cmdBuff, m_omniShadowPipeline.layout, stageFlags, 0,
                          sizeof(pushConst2M), &pushConst2M);

        auto mesh_info = m_pScnMgr->GetMeshInfo(inst.mesh_id);
        vkCmdDrawIndexed(a_cmdBuff, mesh_info.m_indNum, 1, mesh_info.m_indexOffset, mesh_info.m_vertexOffset, 0);
      }
    }
    vkCmdEndRenderPass(a_cmdBuff);
		// Make sure color writes to the framebuffer are finished before using it as transfer source
		vk_utils::setImageLayout(
			a_cmdBuff,
			m_omniShadowBuffer.albedo.image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

		VkImageSubresourceRange cubeFaceSubresourceRange = {};
		cubeFaceSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		cubeFaceSubresourceRange.baseMipLevel = 0;
		cubeFaceSubresourceRange.levelCount = 1;
		cubeFaceSubresourceRange.baseArrayLayer = faceIndex;
		cubeFaceSubresourceRange.layerCount = 1;

		// Change image layout of one cubemap face to transfer destination
		vk_utils::setImageLayout(
			a_cmdBuff,
			m_omniShadowImage.image,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			cubeFaceSubresourceRange);

		// Copy region for transfer from framebuffer to cube face
		VkImageCopy copyRegion = {};

		copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.srcSubresource.baseArrayLayer = 0;
		copyRegion.srcSubresource.mipLevel = 0;
		copyRegion.srcSubresource.layerCount = 1;
		copyRegion.srcOffset = { 0, 0, 0 };

		copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.dstSubresource.baseArrayLayer = faceIndex;
		copyRegion.dstSubresource.mipLevel = 0;
		copyRegion.dstSubresource.layerCount = 1;
		copyRegion.dstOffset = { 0, 0, 0 };

		copyRegion.extent.width = m_shadowWidth;
		copyRegion.extent.height = m_shadowHeight;
		copyRegion.extent.depth = 1;

		// Put image copy into command buffer
		vkCmdCopyImage(
			a_cmdBuff,
			m_omniShadowBuffer.albedo.image,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			m_omniShadowImage.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&copyRegion);

		// Transform framebuffer color attachment back
		vk_utils::setImageLayout(
			a_cmdBuff,
			m_omniShadowBuffer.albedo.image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		// Change image layout of copied face to shader read
		vk_utils::setImageLayout(
			a_cmdBuff,
			m_omniShadowImage.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			cubeFaceSubresourceRange);
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
    if(m_frameFences[i] != VK_NULL_HANDLE)
    {
      vkDestroyFence(m_device, m_frameFences[i], nullptr);
      m_frameFences[i] = VK_NULL_HANDLE;
    }
  }
  m_frameFences.clear();

  vk_utils::deleteImg(m_device, &m_depthBuffer);

  for (size_t i = 0; i < m_frameBuffers.size(); i++)
  {
    if(m_frameBuffers[i] != VK_NULL_HANDLE)
    {
      vkDestroyFramebuffer(m_device, m_frameBuffers[i], nullptr);
      m_frameBuffers[i] = VK_NULL_HANDLE;
    }
  }
  m_frameBuffers.clear();

  vkDestroyFramebuffer(m_device,m_quadFrameBuffer,nullptr);

  if(m_gBuffer.frameBuffer != VK_NULL_HANDLE)
  {
    vkDestroyFramebuffer(m_device, m_gBuffer.frameBuffer, nullptr);
    m_gBuffer.frameBuffer = VK_NULL_HANDLE;
  }

  if(m_omniShadowBuffer.frameBuffer != VK_NULL_HANDLE)
  {
    vkDestroyFramebuffer(m_device, m_omniShadowBuffer.frameBuffer, nullptr);
    m_omniShadowBuffer.frameBuffer = VK_NULL_HANDLE;
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

  if(m_gBuffer.renderPass != VK_NULL_HANDLE)
  {
    vkDestroyRenderPass(m_device, m_gBuffer.renderPass, nullptr);
    m_gBuffer.renderPass = VK_NULL_HANDLE;
  }

  if(m_omniShadowBuffer.renderPass != VK_NULL_HANDLE)
  {
    vkDestroyRenderPass(m_device, m_omniShadowBuffer.renderPass, nullptr);
    m_omniShadowBuffer.renderPass = VK_NULL_HANDLE;
  }

  // if(m_resolvePipeline.pipeline != VK_NULL_HANDLE)
  // {
  //   vkDestroyRenderPass(m_device, m_resolvePipeline.pipeline, nullptr);
  //   m_resolvePipeline.pipeline = VK_NULL_HANDLE;
  // }

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
    // setObjectName(m_cmdBuffersOmniShadow[i], VK_OBJECT_TYPE_COMMAND_BUFFER, "Build omniShadow Recreate");
    // BuildOmniShadowCommandBuffer(m_cmdBuffersOmniShadow[i], m_omniShadowBuffer.frameBuffer, m_swapchain.GetAttachment(i).view,
    //                        m_omniShadowPipeline.pipeline);
    setObjectName(m_cmdBuffersGbuffer[i], VK_OBJECT_TYPE_COMMAND_BUFFER, "Build g-buffer Recreate");
    BuildGbufferCommandBuffer(m_cmdBuffersGbuffer[i], m_gBuffer.frameBuffer, m_swapchain.GetAttachment(i).view,
                           m_gBufferPipeline.pipeline);
    setObjectName(m_cmdBuffersDrawMain[i], VK_OBJECT_TYPE_COMMAND_BUFFER, "Build Resolve Recreate");                        
    BuildResolveCommandBuffer(m_cmdBuffersDrawMain[i], m_frameBuffers[i], m_swapchain.GetAttachment(i).view,
                           m_resolvePipeline.pipeline);
  }

  // *** ray tracing resources
  m_raytracedImageData.resize(m_width * m_height);
  m_pRayTracerCPU = nullptr;
  m_pRayTracerGPU = nullptr;
  SetupRTImage();
  SetupHistoryImages();
  //SetupQuadRenderer();
  //SetupQuadDescriptors();

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
    //SetupQuadRenderer();


    for (uint32_t i = 0; i < m_framesInFlight; ++i)
    {
      // setObjectName(m_cmdBuffersOmniShadow[i], VK_OBJECT_TYPE_COMMAND_BUFFER, "Build omniShadow LoadScene");
      // BuildOmniShadowCommandBuffer(m_cmdBuffersOmniShadow[i], m_omniShadowBuffer.frameBuffer, m_swapchain.GetAttachment(i).view,
      //                       m_omniShadowPipeline.pipeline);
      setObjectName(m_cmdBuffersGbuffer[i], VK_OBJECT_TYPE_COMMAND_BUFFER, "Build g-buffer PI");
      BuildGbufferCommandBuffer(m_cmdBuffersGbuffer[i], m_gBuffer.frameBuffer, m_swapchain.GetAttachment(i).view,
                           m_gBufferPipeline.pipeline);
      setObjectName(m_cmdBuffersDrawMain[i], VK_OBJECT_TYPE_COMMAND_BUFFER, "Build Resolve PI");
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
  m_uniforms.prevProjView = pushConst2M.projView; // set here a prevProjView matrix
  const float aspect   = float(m_width) / float(m_height);
  auto mProjFix        = OpenglToVulkanProjectionMatrixFix();
  auto mProj           = projectionMatrix(m_cam.fov, aspect, 0.1f, 1000.0f);
  auto mLookAt         = LiteMath::lookAt(m_cam.pos, m_cam.lookAt, m_cam.up);
  auto mWorldViewProj = LiteMath::float4x4();
  if (m_uniforms.settings.x)
  { 
    m_uniforms.m_cur_prev_jiiter.z = prevJitter.x;
    m_uniforms.m_cur_prev_jiiter.w = prevJitter.y;
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<std::mt19937::result_type> dist6(0,7);
    vec2 jitter = ((HALTON_SEQUENCE[dist6(rng) % HALTON_COUNT]) - 0.5f) * JITTER_SCALE / vec2(m_width, m_height);
    float4x4 JitterMat = LiteMath::float4x4();
    JitterMat(0,3) = jitter.x;
    JitterMat(1,3) = jitter.y;
    mWorldViewProj = mProjFix * JitterMat * mProj * mLookAt;
    m_uniforms.m_cur_prev_jiiter.x = jitter.x;
    m_uniforms.m_cur_prev_jiiter.y = jitter.y;
    prevJitter = jitter;
  }
  else
  {
    mWorldViewProj = mProjFix * mProj * mLookAt;
  }
  m_prevProjViewMatrix = m_uniforms.prevProjView; 
  m_inverseProjViewMatrix = LiteMath::inverse4x4(mWorldViewProj);
  m_projectionMatrix = mWorldViewProj;
  LiteMath::float4x4 curVehMat = m_pScnMgr->GetVehicleInstanceMatrix(0);
  LiteMath::float4x4 prevVehMat = m_pScnMgr->GetVehiclePrevInstanceMatrix(0);     
  curVehMat.set_col(0, curVehMat.get_col(0) - prevVehMat.get_col(0));
  curVehMat.set_col(1, curVehMat.get_col(1) - prevVehMat.get_col(1));
  curVehMat.set_col(2, curVehMat.get_col(2) - prevVehMat.get_col(2));
  curVehMat.set_col(3, curVehMat.get_col(3) - prevVehMat.get_col(3));
  //m_inverseTransMatrix = LiteMath::inverse4x4(curVehMat);
  if (!forceHistory)
  {
    m_inverseTransMatrix = curVehMat;
    //m_inverseTransMatrix = LiteMath::inverse4x4(curVehMat);
  }
  pushConst2M.projView = mWorldViewProj;
  pushConst2M.lightView = LiteMath::float4x4();
  m_uniforms.invProjView = m_inverseProjViewMatrix;
  //m_inverseProjViewMatrix = mWorldViewProj;
}

void SimpleRender::LoadScene(const char* path)
{
  m_pScnMgr->InstanceVehicle(float3(40.0, 10.0, -20.0), 5.0f, 4.0f);
  m_pScnMgr->LoadScene(path);

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
    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             100},
    {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     100}
  };

  // set large a_maxSets, because every window resize will cause the descriptor set for quad being to be recreated
  m_pBindings = std::make_shared<vk_utils::DescriptorMaker>(m_device, dtypes, 1000);
  
  SetupNoiseImage();
  SetupRTImage();
  //SetupOmniShadowImage();
  SetupHistoryImages();

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

  // for (uint32_t i = 0; i < m_framesInFlight; ++i)
  // {
  //   setObjectName(m_cmdBuffersGbuffer[i], VK_OBJECT_TYPE_COMMAND_BUFFER, "Build g-buffer LoadScene");
  //   BuildGbufferCommandBuffer(m_cmdBuffersGbuffer[i], m_gBuffer.frameBuffer, m_swapchain.GetAttachment(i).view,
  //                          m_gBufferPipeline.pipeline);
  //   setObjectName(m_cmdBuffersDrawMain[i], VK_OBJECT_TYPE_COMMAND_BUFFER, "Build resolve LoadScene");
  //   BuildResolveCommandBuffer(m_cmdBuffersDrawMain[i], m_frameBuffers[i], m_swapchain.GetAttachment(i).view,
  //                          m_resolvePipeline.pipeline);
  // }
}

void SimpleRender::DrawFrameSimple(float a_time)
{
  vkWaitForFences(m_device, 1, &m_frameFences[m_presentationResources.currentFrame], VK_TRUE, UINT64_MAX);
  vkResetFences(m_device, 1, &m_frameFences[m_presentationResources.currentFrame]);

  uint32_t imageIdx;
  m_swapchain.AcquireNextImage(m_presentationResources.imageAvailable, &imageIdx);

  auto currentResolveCmdBuf = m_cmdBuffersDrawMain[m_presentationResources.currentFrame];
  auto currentGbufferCmdBuf = m_cmdBuffersGbuffer[m_presentationResources.currentFrame];
  auto currentRTCmdBuf = m_cmdBuffersRT[m_presentationResources.currentFrame];

  VkSemaphore waitSemaphores[] = {m_presentationResources.imageAvailable};
  VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

  if(m_currentRenderMode == RenderMode::RASTERIZATION)
  {
    setObjectName(currentGbufferCmdBuf, VK_OBJECT_TYPE_COMMAND_BUFFER, "Build g-buffer DrawFrameSimple");
    BuildGbufferCommandBuffer(currentGbufferCmdBuf, m_gBuffer.frameBuffer, m_swapchain.GetAttachment(imageIdx).view,
    m_gBufferPipeline.pipeline);
    
    RayTraceGPU(currentRTCmdBuf, a_time, m_needUpdate);
  }
  // else if(m_currentRenderMode == RenderMode::RAYTRACING)
  // {
  //   if (ENABLE_HARDWARE_RT)
  //     RayTraceGPU(currentRTCmdBuf, a_time);
  //   else
  //     RayTraceCPU();

  //   //BuildCommandBufferQuad(currentResolveCmdBuf, m_swapchain.GetAttachment(imageIdx).view);
  // }

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

  setObjectName(currentResolveCmdBuf, VK_OBJECT_TYPE_COMMAND_BUFFER, "Build resolve DrawFrameSimple");
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
  m_pScnMgr->BuildTLAS(true);
}

void SimpleRender::Cleanup()
{
  m_pGUIRender = nullptr;
  m_pResolveImage = nullptr;
  m_pTaaImage = nullptr;
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

  if(m_resolveImageSampler != VK_NULL_HANDLE)
  {
    vkDestroySampler(m_device, m_resolveImageSampler, nullptr);
    m_resolveImageSampler = VK_NULL_HANDLE;
  }
  vk_utils::deleteImg(m_device, &m_resolveImage);

  // if(m_omniShadowImageSampler != VK_NULL_HANDLE)
  // {
  //   vkDestroySampler(m_device, m_omniShadowImageSampler, nullptr);
  //   m_omniShadowImageSampler = VK_NULL_HANDLE;
  // }
  // vk_utils::deleteImg(m_device, &m_omniShadowImage);

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

  if (m_omniShadowPipeline.pipeline != VK_NULL_HANDLE)
  {
    vkDestroyPipeline(m_device, m_omniShadowPipeline.pipeline, nullptr);
    m_omniShadowPipeline.pipeline = VK_NULL_HANDLE;
  }
  if (m_omniShadowPipeline.layout != VK_NULL_HANDLE)
  {
    vkDestroyPipelineLayout(m_device, m_omniShadowPipeline.layout, nullptr);
    m_omniShadowPipeline.layout = VK_NULL_HANDLE;
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

  if (m_presentationResources.rtFinished != VK_NULL_HANDLE)
  {
    vkDestroySemaphore(m_device, m_presentationResources.rtFinished, nullptr);
    m_presentationResources.rtFinished = VK_NULL_HANDLE;
  }
  ClearBuffer(m_gBuffer);
  ClearBuffer(m_omniShadowBuffer);

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
  prevLightPos = m_uniforms.lights[0].pos;
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
    //ImGui::SliderFloat3("Light source 1 position", m_uniforms.lights[0].pos.M, -100.f, 100.f);
    ImGui::SliderFloat3("Light source 1 dir", m_uniforms.lights[0].dir.M, -1.0f, 1.0f);
    ImGui::SliderFloat("Light source 1 radius", &m_uniforms.lights[0].radius_lightDist_dummies.x, 0.0f, 2.0f);
    ImGui::SliderInt("FaceIndex", &gbuffer_index, 0, 10); //0 no debug, 1 pos, 2 normal, 3 albedo, 4 shadow, 5 velocity
    ImGui::Checkbox("Taa", &taaFlag);
    ImGui::Checkbox("TurnOff", &softShadow);
    ImGui::Checkbox("NeedUpdate", &m_needUpdateSlider);
    ImGui::Checkbox("ForceHistory ", &forceHistory);
    ImGui::Checkbox("teleport ", &teleport);
    
    float4 curVehPos = m_pScnMgr->GetVehicleInstanceMatrix(0).get_col(3);
    float4 prevVehPos = m_pScnMgr->GetVehiclePrevInstanceMatrix(0).get_col(3);
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::Text("CurrCarPos: x=%.3f, y=%.3f, z= %.3f",curVehPos.x,curVehPos.y,curVehPos.z);
    ImGui::Text("PrevCarPos: x=%.3f, y=%.3f, z= %.3f",prevVehPos.x,prevVehPos.y,prevVehPos.z);
    ImGui::NewLine();
    ImGui::Text("CamPos: x=%.3f, y=%.3f, z= %.3f",GetCurrentCamera().pos.x,GetCurrentCamera().pos.y,GetCurrentCamera().pos.z);
    ImGui::NewLine();
    ImGui::Text("CamDir: x=%.3f, y=%.3f, z= %.3f",GetCurrentCamera().lookAt.x,GetCurrentCamera().lookAt.y,GetCurrentCamera().lookAt.z);
    ImGui::Text("CamDirNorm: x=%.3f, y=%.3f, z= %.3f",GetCurrentCamera().forward().x,GetCurrentCamera().forward().y,GetCurrentCamera().forward().z);
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),"Press 'B' to recompile and reload shaders");
    ImGui::Text("Changing bindings is not supported.");
    ImGui::Text("Vertex shader path: %s", VERTEX_SHADER_PATH.c_str());
    ImGui::Text("Fragment shader path: %s", MRT_FRAGMENT_SHADER_PATH.c_str());
    ImGui::End();
  }
  currentLightPos = m_uniforms.lights[0].pos;
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
  auto currentRTCmdBuf = m_cmdBuffersRT[m_presentationResources.currentFrame];

  VkSemaphore waitSemaphores[] = {m_presentationResources.imageAvailable};
  VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};


  setObjectName(currentGbufferCmdBuf, VK_OBJECT_TYPE_COMMAND_BUFFER, "Build g-buffer DrawFrameWithGUI");
  BuildGbufferCommandBuffer(currentGbufferCmdBuf, m_gBuffer.frameBuffer, m_swapchain.GetAttachment(imageIdx).view,
  m_gBufferPipeline.pipeline);
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

  m_needUpdate = m_needUpdateSlider ? 1U : 0U;
  RayTraceGPU(currentRTCmdBuf,a_time, m_needUpdate);

  waitSemaphores[0] = m_presentationResources.gbufferFinished;
  submitInfo.pWaitSemaphores = waitSemaphores;
  waitStages[0] = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
  submitInfo.pWaitDstStageMask = waitStages;
  signalSemaphores[0] = m_presentationResources.rtFinished;
  submitInfo.pSignalSemaphores = signalSemaphores;
  submitInfo.pCommandBuffers = &currentRTCmdBuf;
  VK_CHECK_RESULT(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, nullptr));

  setObjectName(currentResolveCmdBuf, VK_OBJECT_TYPE_COMMAND_BUFFER, "Build resolve DrawFrameWithGUI");
  BuildResolveCommandBuffer(currentResolveCmdBuf, m_frameBuffers[imageIdx], m_swapchain.GetAttachment(imageIdx).view,
                           m_resolvePipeline.pipeline);

  ImDrawData* pDrawData = ImGui::GetDrawData();
  auto currentGUICmdBuf = m_pGUIRender->BuildGUIRenderCommand(imageIdx, pDrawData);

  std::vector<VkCommandBuffer> submitCmdBufs = { currentResolveCmdBuf, currentGUICmdBuf};

  submitInfo.commandBufferCount = (uint32_t)submitCmdBufs.size();
  submitInfo.pCommandBuffers = submitCmdBufs.data();

  waitSemaphores[0] = m_presentationResources.rtFinished;
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

void SimpleRender::ClearBuffer(FrameBuffer buffer)
{
  vkDestroyImage(m_device, buffer.albedo.image, nullptr);
  vkDestroyImage(m_device, buffer.position.image, nullptr);
  vkDestroyImage(m_device, buffer.normal.image, nullptr);
  vkDestroyImage(m_device, buffer.depth.image, nullptr);
  vkDestroyImage(m_device, buffer.velocity.image, nullptr);
  vkDestroyImageView(m_device, buffer.albedo.view, nullptr);
  vkDestroyImageView(m_device, buffer.position.view, nullptr);
  vkDestroyImageView(m_device, buffer.normal.view, nullptr);
  vkDestroyImageView(m_device, buffer.depth.view, nullptr);
  vkDestroyImageView(m_device, buffer.velocity.view, nullptr);
  vkFreeMemory(m_device, buffer.albedo.mem, nullptr);
  vkFreeMemory(m_device, buffer.position.mem, nullptr);
  vkFreeMemory(m_device, buffer.normal.mem, nullptr);
  vkFreeMemory(m_device, buffer.depth.mem, nullptr);
  vkFreeMemory(m_device, buffer.velocity.mem, nullptr);
}
