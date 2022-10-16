#ifndef SIMPLE_RENDER_H
#define SIMPLE_RENDER_H

#define VK_NO_PROTOTYPES

// #define STB_IMAGE_IMPLEMENTATION
// #include "stb_image.h"

#include "../../render/scene_mgr.h"
#include "../../render/render_common.h"
#include "../../render/render_gui.h"
#include "../../../resources/shaders/common.h"
#include <geom/vk_mesh.h>
#include <vk_descriptor_sets.h>
#include <vk_fbuf_attachment.h>
#include <vk_quad.h>
#include <vk_images.h>
#include <vk_swapchain.h>
#include <string>
#include <iostream>
#include <random>
#include <render/CrossRT.h>
#include "raytracing.h"
#include "raytracing_generated.h"

//delete after 
#include <vector>
#include <fstream>
#include <cstring>

enum class RenderMode
{
  RASTERIZATION,
  RAYTRACING,
};

class RayTracer_GPU : public RayTracer_Generated
{
public:
  RayTracer_GPU(int32_t a_width, uint32_t a_height) : RayTracer_Generated(a_width, a_height) {} 
  std::string AlterShaderPath(const char* a_shaderPath) override { return std::string("../../src/samples/raytracing/") + std::string(a_shaderPath); }
  void InitDescriptors(std::shared_ptr<SceneManager> sceneManager, vk_utils::VulkanImageMem noiseMapTex, VkSampler noiseTexSampler);
  //void InitDescriptors(std::shared_ptr<SceneManager> sceneManager);
};

class SimpleRender : public IRender
{
public:
  const std::string VERTEX_SHADER_PATH   = "../resources/shaders/simple.vert";
  const std::string FRAGMENT_SHADER_PATH = "../resources/shaders/simple.frag";
  const std::string TAA_VERTEX_SHADER_PATH = "../resources/shaders/taa.vert";
  const std::string TAA_FRAGMENT_SHADER_PATH = "../resources/shaders/taa.frag";
  const std::string SOFT_RT_SHADOWS_VERTEX_SHADER_PATH = "../resources/shaders/softRT.vert";
  const std::string SOFT_RT_SHADOWS_FRAGMENT_SHADER_PATH = "../resources/shaders/softRT.frag";
  const std::string RESOLVE_FRAGMENT_SHADER_PATH = "../resources/shaders/resolve.frag";
  const std::string RESOLVE_VERTEX_SHADER_PATH = "../resources/shaders/resolve.vert";
  const std::string OMNI_SHADOW_FRAGMENT_SHADER_PATH = "../resources/shaders/omnishadow.frag";
  const std::string OMNI_SHADOW_VERTEX_SHADER_PATH = "../resources/shaders/omnishadow.vert";
  const std::string MRT_FRAGMENT_SHADER_PATH = "../resources/shaders/mrt.frag";
  const std::string RESULT_FRAGMENT_SHADER_PATH = "../resources/shaders/result.frag";
  const std::string RESULT_VERTEX_SHADER_PATH = "../resources/shaders/result.vert";
  const std::string MEDIAN_VERTEX_SHADER_PATH = "../resources/shaders/quad3_vert.vert";
  const std::string MEDIAN_FRAGMENT_SHADER_PATH = "../resources/shaders/median.frag";
  const std::string BLUR_FRAGMENT_SHADER_PATH = "../resources/shaders/blur.frag";
  const std::string KUWAHARA_FRAGMENT_SHADER_PATH = "../resources/shaders/kuwahara.frag";
  const std::string SHARP_FRAGMENT_SHADER_PATH = "../resources/shaders/sharp.frag";

  

  const std::string NOISE_TEX = "../resources/textures/STBN.png";
  
  const bool        ENABLE_HARDWARE_RT   = true;

  static constexpr uint64_t STAGING_MEM_SIZE = 16 * 16 * 1024u;

  SimpleRender(uint32_t a_width, uint32_t a_height);
  ~SimpleRender()  { Cleanup(); };

  inline uint32_t     GetWidth()      const override { return m_width; }
  inline uint32_t     GetHeight()     const override { return m_height; }
  inline VkInstance   GetVkInstance() const override { return m_instance; }
  void InitVulkan(const char** a_instanceExtensions, uint32_t a_instanceExtensionsCount, uint32_t a_deviceId) override;

  void InitPresentation(VkSurfaceKHR& a_surface) override;

  void ProcessInput(const AppInput& input) override;
  void UpdateCamera(const Camera* cams, uint32_t a_camsCount) override;
  Camera GetCurrentCamera() override {return m_cam;}
  void UpdateView();

  void LoadScene(const char *path) override;
  void DrawFrame(float a_time, DrawMode a_mode) override;

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  // debugging utils
  //
  PFN_vkSetDebugUtilsObjectNameEXT SetDebugUtilsObjectNameEXT = nullptr;
  template<class T>
  void setObjectName(T handle, VkObjectType type, const char* name)
  {
    if (SetDebugUtilsObjectNameEXT != nullptr)
    {
		  VkDebugUtilsObjectNameInfoEXT nameInfo{
		    .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .pNext = NULL,
		    .objectType = type,
		    .objectHandle = reinterpret_cast<uint64_t>(handle),
		    .pObjectName = name,
		  };
		  SetDebugUtilsObjectNameEXT(m_device, &nameInfo);
    }
  }

  static VKAPI_ATTR VkBool32 VKAPI_CALL debugReportCallbackFn(
    VkDebugReportFlagsEXT                       flags,
    VkDebugReportObjectTypeEXT                  objectType,
    uint64_t                                    object,
    size_t                                      location,
    int32_t                                     messageCode,
    const char* pLayerPrefix,
    const char* pMessage,
    void* pUserData)
  {
    (void)flags;
    (void)objectType;
    (void)object;
    (void)location;
    (void)messageCode;
    (void)pUserData;
    std::cout << pLayerPrefix << ": " << pMessage << std::endl;
    return VK_FALSE;
  }

  VkDebugReportCallbackEXT m_debugReportCallback = nullptr;
protected:

  VkInstance       m_instance       = VK_NULL_HANDLE;
  VkCommandPool    m_commandPool    = VK_NULL_HANDLE;
  VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
  VkDevice         m_device         = VK_NULL_HANDLE;
  VkQueue          m_graphicsQueue  = VK_NULL_HANDLE;
  VkQueue          m_transferQueue  = VK_NULL_HANDLE;

  std::shared_ptr<vk_utils::ICopyEngine> m_pCopyHelper;

  vk_utils::QueueFID_T m_queueFamilyIDXs {UINT32_MAX, UINT32_MAX, UINT32_MAX};

  RenderMode m_currentRenderMode = RenderMode::RASTERIZATION;

  struct
  {
    uint32_t    currentFrame      = 0u;
    VkQueue     queue             = VK_NULL_HANDLE;
    VkSemaphore imageAvailable    = VK_NULL_HANDLE;
    VkSemaphore renderingFinished = VK_NULL_HANDLE;
    VkSemaphore gbufferFinished = VK_NULL_HANDLE;
  } m_presentationResources;

  std::vector<VkFence> m_frameFences;
  std::vector<VkCommandBuffer> m_cmdBuffersDrawMain;
  std::vector<VkCommandBuffer> m_cmdBuffersGbuffer;


  struct
  {
    LiteMath::float4x4 projView;
    LiteMath::float4x4 model;
    LiteMath::float4x4 lightView;
    LiteMath::float4 color;
    LiteMath::float2 screenSize;
  } pushConst2M;

  struct FrameBufferAttachment {
  VkImage image;
  VkDeviceMemory mem;
  VkImageView view;
  VkFormat format;
	};
	struct FrameBuffer {
		int32_t width, height;
		VkFramebuffer frameBuffer;
		FrameBufferAttachment position, normal, albedo;
    FrameBufferAttachment velocity;
		FrameBufferAttachment depth;
		VkRenderPass renderPass;
	} m_gBuffer, m_omniShadowBuffer;

  struct InputControlMouseEtc
  {
    bool drawFSQuad = false;
  } m_input;

  VkSampler m_colorSampler;

  UniformParams m_uniforms {};
  VkBuffer m_ubo = VK_NULL_HANDLE;
  VkDeviceMemory m_uboAlloc = VK_NULL_HANDLE;
  void* m_uboMappedMem = nullptr;
  int faceIndex = 0;
  int gbuffer_index = 0;
  bool taaFlag;
  bool softShadow;
  bool onlyOneLoadOfShadows = true;
  vec4 currentLightPos, prevLightPos;
  std::shared_ptr<vk_utils::DescriptorMaker> m_pBindings = nullptr;

  pipeline_data_t m_basicForwardPipeline {};
  pipeline_data_t m_resolvePipeline {};
  pipeline_data_t m_gBufferPipeline {};
  pipeline_data_t m_omniShadowPipeline {};
  pipeline_data_t m_taaPipeline {};
  pipeline_data_t m_filterPipeline {};
  pipeline_data_t m_softShadowPipeline {};

  VkDescriptorSet m_dSet = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_dSetLayout = VK_NULL_HANDLE;
  VkDescriptorSet m_dOmniShadowSet = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_dOmniShadowSetLayout = VK_NULL_HANDLE;
  VkDescriptorSet m_dResolveSet = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_dResolveSetLayout = VK_NULL_HANDLE;
  VkDescriptorSet m_dTAASet = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_dTAASetLayout = VK_NULL_HANDLE;
  VkDescriptorSet m_dSoftRTSet = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_dSoftRTSetLayout = VK_NULL_HANDLE;
  VkDescriptorSet m_dFilterSet = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_dFilterSetLayout = VK_NULL_HANDLE;
  VkDescriptorSet m_dResultSet = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_dResultSetLayout = VK_NULL_HANDLE;
  VkRenderPass m_screenRenderPass = VK_NULL_HANDLE; // rasterization renderpass

  LiteMath::float4x4 m_projectionMatrix;
  LiteMath::float4x4 m_inverseProjViewMatrix;
  float2 prevJitter = float2(0.0, 0.0f);

  // *** ray tracing
  // full screen quad resources to display ray traced image
  void GetRTFeatures();
  void * m_pDeviceFeatures;
  VkPhysicalDeviceAccelerationStructureFeaturesKHR m_accelStructFeatures{};
  VkPhysicalDeviceAccelerationStructureFeaturesKHR m_enabledAccelStructFeatures{};
  VkPhysicalDeviceBufferDeviceAddressFeatures m_enabledDeviceAddressFeatures{};
  VkPhysicalDeviceRayQueryFeaturesKHR m_enabledRayQueryFeatures;

  std::vector<uint32_t> m_raytracedImageData;
  std::shared_ptr<vk_utils::IQuad> m_pFSQuad;
  VkDescriptorSet m_quadDS = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_quadDSLayout = VK_NULL_HANDLE;
  vk_utils::VulkanImageMem m_rtImage;
  VkSampler                m_rtImageSampler = VK_NULL_HANDLE;

  vk_utils::VulkanImageMem m_omniShadowImage;
  VkSampler                m_omniShadowImageSampler = VK_NULL_HANDLE;

  vk_utils::VulkanImageMem m_resolveImage;
  VkSampler                m_resolveImageSampler = VK_NULL_HANDLE;

  vk_utils::VulkanImageMem m_prevDepthImage;
  VkSampler                m_prevDepthImageSampler = VK_NULL_HANDLE;

  vk_utils::VulkanImageMem m_prevFrameImage;
  VkSampler                m_prevFrameImageSampler = VK_NULL_HANDLE;

  vk_utils::VulkanImageMem m_prevRTImage;
  VkSampler                m_prevRTImageSampler = VK_NULL_HANDLE;

  vk_utils::VulkanImageMem m_taaImage;
  VkSampler                m_taaImageSampler = VK_NULL_HANDLE;

  vk_utils::VulkanImageMem m_resImage;
  VkSampler                m_resImageSampler = VK_NULL_HANDLE;
  pipeline_data_t m_quadPipeline;
  VkRenderPass m_quadRenderPass = VK_NULL_HANDLE; 
  VkRenderPass m_resolveRenderPass = VK_NULL_HANDLE; 

  VkFramebuffer    m_quadFrameBuffer = VK_NULL_HANDLE;
  VkImageView      m_quadTargetView;

  VkDescriptorSet m_finalQuadDS = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_finalQuadDSLayout = VK_NULL_HANDLE;

  int filterRadius = 0;
 

  std::shared_ptr<ISceneObject> m_pAccelStruct = nullptr;
  std::unique_ptr<RayTracer> m_pRayTracerCPU;
  std::unique_ptr<RayTracer_GPU> m_pRayTracerGPU;
  std::shared_ptr<vk_utils::RenderTarget>        m_pResolveImage;
  VkDeviceMemory        m_memResolveImage = VK_NULL_HANDLE;
  uint32_t m_resolveImageId = 0;

  std::shared_ptr<vk_utils::RenderTarget>        m_pTaaImage;
  VkDeviceMemory        m_memTaaImage = VK_NULL_HANDLE;

  std::shared_ptr<vk_utils::RenderTarget>        m_pFilterImage;
  VkDeviceMemory        m_memFilterImage = VK_NULL_HANDLE;

  std::shared_ptr<vk_utils::RenderTarget>        m_pSoftRTImage;
  VkDeviceMemory        m_memSoftRTImage = VK_NULL_HANDLE;
  uint32_t m_taaImageId = 0;          

  void RayTraceCPU();
  void RayTraceGPU(float a_time);

  VkBuffer m_genColorBuffer = VK_NULL_HANDLE;
  VkDeviceMemory m_colorMem = VK_NULL_HANDLE;
  //

  // *** presentation
  VkSurfaceKHR m_surface = VK_NULL_HANDLE;
  VulkanSwapChain m_swapchain;
  std::vector<VkFramebuffer> m_frameBuffers;
  vk_utils::VulkanImageMem m_depthBuffer{};
  // ***

  void SetupNoiseImage();
  vk_utils::VulkanImageMem m_NoiseMapTex{};
  VkSampler m_NoiseTexSampler = VK_NULL_HANDLE;

  // *** GUI
  std::shared_ptr<IRenderGUI> m_pGUIRender;
  void SetupGUIElements();
  void DrawFrameWithGUI(float a_time);
  //

  Camera   m_cam;
  uint32_t m_width  = 1024u;
  uint32_t m_height = 1024u;

  uint32_t m_shadowWidth  = 1024u;
  uint32_t m_shadowHeight = 1024u;

  uint32_t m_framesInFlight  = 2u;
  bool m_vsync = false;
  const int HALTON_COUNT = 8;
  const vec2 HALTON_SEQUENCE[8] = {
    vec2(1.0 / 2.0, 1.0 / 3.0),
    vec2(1.0 / 4.0, 2.0 / 3.0),
    vec2(3.0 / 4.0, 1.0 / 9.0),
    vec2(1.0 / 8.0, 4.0 / 9.0),
    vec2(5.0 / 8.0, 7.0 / 9.0),
    vec2(3.0 / 8.0, 2.0 / 9.0),
    vec2(7.0 / 8.0, 5.0 / 9.0),
    vec2(1.0 / 16.0, 8.0 / 9.0),
    
  };

  // const int HALTON_COUNT = 16;
  // const vec2 HALTON_SEQUENCE[16] = {
  //      vec2(0.5f, 0.33333333f),
  //      vec2(0.25f, 0.66666667f),
  //      vec2(0.75f, 0.11111111f),
  //      vec2(0.125f, 0.44444444f),
  //      vec2(0.625f, 0.77777778f),
  //      vec2(0.375f, 0.22222222f),
  //      vec2(0.875f, 0.55555556f),
  //      vec2(0.0625f, 0.88888889f),
  //      vec2(0.5625f, 0.03703704f),
  //      vec2(0.3125f, 0.37037037f),
  //      vec2(0.8125f, 0.7037037f),
  //      vec2(0.1875f, 0.14814815f),
  //      vec2(0.6875f, 0.48148148f),
  //      vec2(0.4375f, 0.81481481f),
  //      vec2(0.9375f, 0.25925926f),
  //      vec2(0.03125f, 0.59259259f),
  //      vec2(0.53125f, 0.92592593f),
  //      vec2(0.28125f, 0.07407407f),
  //};

  VkPhysicalDeviceFeatures m_enabledDeviceFeatures = {};
  std::vector<const char*> m_deviceExtensions      = {};
  std::vector<const char*> m_optionalDeviceExtensions      = {};
  std::vector<const char*> m_instanceExtensions    = {};

  bool m_enableValidation;
  std::vector<const char*> m_validationLayers;

  std::shared_ptr<SceneManager> m_pScnMgr = nullptr;

  void DrawFrameSimple(float a_time);
  void SetupOmniShadow();
  void SetupGbuffer();

  void CreateAttachment(
  VkFormat format,
  VkImageUsageFlagBits imageUsageType,
  VkImageUsageFlags usage,
  FrameBufferAttachment *attachment, float with, float height);

  void CreateInstance();
  void CreateDevice(uint32_t a_deviceId);

  void UpdateCubeFace(uint32_t faceIndex, VkCommandBuffer cmdBuff);

  void BuildCommandBufferSimple(VkCommandBuffer cmdBuff, VkFramebuffer frameBuff,
                                VkImageView a_targetImageView, VkPipeline a_pipeline);

  void BuildGbufferCommandBuffer(VkCommandBuffer cmdBuff, VkFramebuffer frameBuff,
                              VkImageView a_targetImageView, VkPipeline a_pipeline);

  void BuildResolveCommandBuffer(VkCommandBuffer cmdBuff, VkFramebuffer frameBuff,
                                VkImageView a_targetImageView, VkPipeline a_pipeline);                               
  void AddCmdsShadowmapPass(VkCommandBuffer a_cmdBuff, VkFramebuffer a_frameBuff);
  // *** Ray tracing related stuff
  void BuildCommandBufferQuad(VkCommandBuffer a_cmdBuff, VkImageView a_targetImageView);
  void SetupQuadRenderer();
  void SetupQuadDescriptors();
  void SetupRTImage();
  void SetupHistoryImages();
  void SetupOmniShadowImage();
  void SetupRTScene();

  uint32_t findMemoryType(uint32_t memoryTypeBits, VkMemoryPropertyFlags properties, VkPhysicalDevice physicalDevice);
  // ***************************

  void SetupSimplePipeline();
  void CleanupPipelineAndSwapchain();
  void RecreateSwapChain();

  void CreateUniformBuffer();
  void UpdateUniformBuffer(float a_time);

  void Cleanup();
  void ClearBuffer(FrameBuffer);

  void SetupDeviceFeatures();
  void SetupDeviceExtensions();
  void SetupValidationLayers();
};


#endif //SIMPLE_RENDER_H
