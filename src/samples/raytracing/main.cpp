#include "simple_render.h"
#include "utils/glfw_window.h"

void initVulkanGLFW(std::shared_ptr<IRender> &app, GLFWwindow* window, int deviceID)
{
  uint32_t glfwExtensionCount = 0;
  const char** glfwExtensions;
  glfwExtensions  = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

  if(glfwExtensions == nullptr)
  {
    std::cout << "WARNING. Can't connect Vulkan to GLFW window (glfwGetRequiredInstanceExtensions returns NULL)" << std::endl;
  }

  app->InitVulkan(glfwExtensions, glfwExtensionCount, deviceID);

  if(glfwExtensions != nullptr)
  {
    VkSurfaceKHR surface;
    VK_CHECK_RESULT(glfwCreateWindowSurface(app->GetVkInstance(), window, nullptr, &surface));
    setupImGuiContext(window);
    app->InitPresentation(surface);
  }
}

int main()
{
  constexpr int WIDTH = 1024;
  constexpr int HEIGHT = 1024;
  constexpr int VULKAN_DEVICE_ID = 0;

  std::shared_ptr<IRender> app = std::make_shared<SimpleRender>(WIDTH, HEIGHT);

  if(app == nullptr)
  {
    std::cout << "Can't create render of specified type" << std::endl;
    return 1;
  }

  auto* window = initWindow(WIDTH, HEIGHT);

  initVulkanGLFW(app, window, VULKAN_DEVICE_ID);

  app->LoadScene("../resources/scenes/01_simple_scenes/instanced_objects.xml");
  //app->LoadScene("../resources/scenes/01_sponza/statex_00001.xml");
  //app->LoadScene("../resources/scenes/02_cry_sponza/statex_00001.xml");
  //app->LoadScene("../resources/scenes/03_san_miguel/statex_00001.xml");
  //app->LoadScene("../resources/scenes/043_cornell_normals/statex_00001.xml");
  //app->LoadScene("../resources/scenes/breakfast_room/statex_00001.xml");
  //app->LoadScene("../resources/scenes/conference/statex_00001.xml");
  //app->LoadScene("../resources/scenes/pillars/pillars.gltf");
  //app->LoadScene("../resources/scenes/buggy/Buggy.gltf");
  //app->LoadScene("../resources/scenes/powerplant/change_00000.xml");
  //app->LoadScene("../resources/scenes/RoadScenelib/statex_00001.xml");
  //app->LoadScene("../resources/scenes/rungholt/statex_00001.xml");
  //app->LoadScene("../resources/scenes/sibenik/statex_00001.xml");
  //app->LoadScene("../resources/scenes/sun_temple/scenelib/statex_00001.xml");
  //app->LoadScene("../resources/scenes/box/Box.gltf");
  //app->LoadScene("../resources/scenes/canyon_landscape/scene.gltf");// big canyon
  //app->LoadScene("../resources/scenes/mars/scene.gltf");
  //app->LoadScene("../resources/scenes/cityscape/scene.gltf"); // really good wihtout ATEST
  //app->LoadScene("../resources/scenes/sponza_gltf/Sponza.gltf"); // sponza gltf
  //app->LoadScene("../resources/scenes/low_poly_city/scene.gltf"); // very big scene run with MATERAL::NONE

  bool showGUI = true;
  mainLoop(app, window, showGUI);

  return 0;
}
