#include "VulkanMain.h"
#include "ValidationLayers.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/cimport.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include <stb/stb_image.h>

const char* APPLICATION_NAME = "Heart_Beat_Threading";

// Android Native App pointer...
android_app* androidAppCtx = nullptr;

// Global Variables ...
struct VulkanDeviceInfo {
  bool initialized_;

  VkInstance instance_;
  VkPhysicalDevice gpuDevice_;
  VkDevice device_;
  uint32_t queueFamilyIndex_;

  VkSurfaceKHR surface_;
  VkQueue queue_;
};
VulkanDeviceInfo device;

struct VulkanSwapchainInfo {
  VkSwapchainKHR swapchain_;
  uint32_t swapchainLength_;

  VkExtent2D displaySize_;
  VkFormat displayFormat_;
  VkSemaphore semaphore_;

  // array of frame buffers and views
  std::vector<VkImage> displayImages_;
  std::vector<VkImageView> displayViews_;
  std::vector<VkFramebuffer> framebuffers_;
};
VulkanSwapchainInfo swapchain;

struct VulkanRenderInfo {
  VkRenderPass renderPass_;
  VkCommandPool cmdPool_;
  std::vector<VkCommandBuffer> cmdBuffer_;
  VkSemaphore semaphore_;
};
VulkanRenderInfo render;

struct VulkanBufferInfo {
  VkBuffer buffer;
  VkDeviceMemory memory;
  VkDescriptorBufferInfo descriptor;
};
VulkanBufferInfo uniformBuffer;

VkDescriptorSet descriptorSet;
VkDescriptorSetLayout descriptorSetLayout;
VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
VkPipelineLayout pipelineLayout;
VkPipelineCache pipelineCache;
VkPipeline gfxPipeline;

struct
{
  VkImage image;
  VkDeviceMemory mem;
  VkImageView view;
  VkFormat format;
} depthStencil;

// Same uniform buffer layout as shader
struct {
  glm::mat4 projectionMatrix;
  glm::mat4 modelMatrix;
  glm::mat4 viewMatrix;
} uboVS;

bool viewChanged;

float zoom = -5.5f;
glm::vec3 rotation = glm::vec3();
glm::vec3 cameraPos = glm::vec3();

struct Vertex {
  glm::vec3 pos;
  glm::vec3 normal;
  glm::vec2 uv;
  glm::vec3 color;
};

struct Model {
  struct {
    VkBuffer buffer;
    VkDeviceMemory memory;
  } vertices;
  struct {
    uint32_t count;
    VkBuffer buffer;
    VkDeviceMemory memory;
  } indices;
  // Destroys all Vulkan resources created for this model
  void destroy(VkDevice device)
  {
    vkDestroyBuffer(device, vertices.buffer, nullptr);
    vkFreeMemory(device, vertices.memory, nullptr);
    vkDestroyBuffer(device, indices.buffer, nullptr);
    vkFreeMemory(device, indices.memory, nullptr);
  };
} model;

struct Texture {
  VkSampler sampler;
  VkImage image;
  VkImageLayout layout;
  VkDeviceMemory memory;
  VkImageView view;
  VkImageType type;
  VkFormat format;
  uint32_t width;
  uint32_t height;
  uint32_t mipLevels;
  uint32_t layerCount;
};

struct Texture heartTexture {
    .type = VK_IMAGE_TYPE_2D,
    .format = VK_FORMAT_R8G8B8A8_UNORM,
};

/*
 * SetImageLayout():
 *    Helper function to transition color buffer layout
 */
void SetImageLayout(VkCommandBuffer cmdBuffer, VkImage image,
                    VkImageLayout oldImageLayout, VkImageLayout newImageLayout,
                    VkPipelineStageFlags srcStages,
                    VkPipelineStageFlags destStages);

// A helper function
bool MapMemoryTypeToIndex(uint32_t typeBits, VkFlags requirements_mask,
                          uint32_t* typeIndex) {
  VkPhysicalDeviceMemoryProperties memoryProperties;
  vkGetPhysicalDeviceMemoryProperties(device.gpuDevice_, &memoryProperties);
  // Search memtypes to find first index with those properties
  for (uint32_t i = 0; i < 32; i++) {
    if ((typeBits & 1) == 1) {
      // Type is available, does it match user properties?
      if ((memoryProperties.memoryTypes[i].propertyFlags & requirements_mask) == requirements_mask) {
        *typeIndex = i;
        return true;
      }
    }
    typeBits >>= 1;
  }
  return false;
}

void updateUniformBuffers(void) {
  uboVS.projectionMatrix = glm::perspective(glm::radians(90.0f),
                                      (float)(swapchain.displaySize_.width) / (float)swapchain.displaySize_.height,
                                      0.01f,
                                      2000.0f);

  uboVS.viewMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, zoom));

  uboVS.modelMatrix = glm::mat4(1.0f);
  uboVS.modelMatrix = glm::rotate(uboVS.modelMatrix, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
  uboVS.modelMatrix = glm::rotate(uboVS.modelMatrix, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
  uboVS.modelMatrix = glm::rotate(uboVS.modelMatrix, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

  uint8_t *pData;
  CALL_VK(vkMapMemory(device.device_, uniformBuffer.memory, 0, sizeof(uboVS), 0, (void **)&pData));
  memcpy(pData, &uboVS, sizeof(uboVS));
  vkUnmapMemory(device.device_, uniformBuffer.memory);
}

VkBool32 getSupportedDepthFormat(VkPhysicalDevice physicalDevice, VkFormat *depthFormat)
{
  // Since all depth formats may be optional, we need to find a suitable depth format to use
  // Start with the highest precision packed format
  std::vector<VkFormat> depthFormats = {
      VK_FORMAT_D32_SFLOAT_S8_UINT,
      VK_FORMAT_D32_SFLOAT,
      VK_FORMAT_D24_UNORM_S8_UINT,
      VK_FORMAT_D16_UNORM_S8_UINT,
      VK_FORMAT_D16_UNORM
  };

  for (auto& format : depthFormats)
  {
    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProps);
    // Format must support depth stencil attachment for optimal tiling
    if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
    {
      *depthFormat = format;
      return true;
    }
  }

  return false;
}

// Create vulkan device
void CreateVulkanDevice(ANativeWindow* platformWindow) {

#ifdef VALIDATION_LAYERS
  // prepare debug and layer objects
  LayerAndExtensions layerAndExt;
  layerAndExt.AddInstanceExt(layerAndExt.GetDbgExtName());
#else
  std::vector<const char*> instance_extensions;
  std::vector<const char*> device_extensions;
  instance_extensions.push_back("VK_KHR_surface");
  instance_extensions.push_back("VK_KHR_android_surface");
  device_extensions.push_back("VK_KHR_swapchain");
#endif

  VkApplicationInfo appInfo = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pNext = nullptr,
      .apiVersion = VK_MAKE_VERSION(1, 0, 0),
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .pApplicationName = APPLICATION_NAME,
      .pEngineName = "NAVS",
  };

  // **********************************************************
  // Create the Vulkan instance
  VkInstanceCreateInfo instanceCreateInfo{
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pNext = nullptr,
      .pApplicationInfo = &appInfo,
#ifdef VALIDATION_LAYERS
      .enabledExtensionCount = layerAndExt.InstExtCount(),
      .ppEnabledExtensionNames = static_cast<const char* const*>(layerAndExt.InstExtNames()),
      .enabledLayerCount = layerAndExt.InstLayerCount(),
      .ppEnabledLayerNames = static_cast<const char* const*>(layerAndExt.InstLayerNames()),
#else
      .enabledExtensionCount = static_cast<uint32_t>(instance_extensions.size()),
      .ppEnabledExtensionNames = instance_extensions.data(),
      .enabledLayerCount = 0,
      .ppEnabledLayerNames = nullptr,
#endif
  };

  CALL_VK(vkCreateInstance(&instanceCreateInfo, nullptr, &device.instance_));

#ifdef VALIDATION_LAYERS
  // Create debug callback obj and connect to vulkan instance
  layerAndExt.HookDbgReportExt(device.instance_);
#endif

  VkAndroidSurfaceCreateInfoKHR createInfo{
      .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
      .pNext = nullptr,
      .flags = 0,
      .window = platformWindow};

  CALL_VK(vkCreateAndroidSurfaceKHR(device.instance_, &createInfo, nullptr, &device.surface_));
  // Find one GPU to use:
  // On Android, every GPU device is equal -- supporting
  // graphics/compute/present
  // for this sample, we use the very first GPU device found on the system
  uint32_t gpuCount = 0;
  CALL_VK(vkEnumeratePhysicalDevices(device.instance_, &gpuCount, nullptr));
  VkPhysicalDevice tmpGpus[gpuCount];
  CALL_VK(vkEnumeratePhysicalDevices(device.instance_, &gpuCount, tmpGpus));
  device.gpuDevice_ = tmpGpus[0];  // Pick up the first GPU Device

#ifdef VALIDATION_LAYERS
  layerAndExt.InitDevLayersAndExt(device.gpuDevice_);
#endif

  // Find a GFX queue family
  uint32_t queueFamilyCount;
  vkGetPhysicalDeviceQueueFamilyProperties(device.gpuDevice_, &queueFamilyCount, nullptr);
  assert(queueFamilyCount);
  std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(device.gpuDevice_, &queueFamilyCount, queueFamilyProperties.data());

  uint32_t queueFamilyIndex;
  for (queueFamilyIndex = 0; queueFamilyIndex < queueFamilyCount;
       queueFamilyIndex++) {
    if (queueFamilyProperties[queueFamilyIndex].queueFlags &
        VK_QUEUE_GRAPHICS_BIT) {
      break;
    }
  }
  assert(queueFamilyIndex < queueFamilyCount);
  device.queueFamilyIndex_ = queueFamilyIndex;

  // Create a logical device (vulkan device)
  float priorities[] = {
      1.0f,
  };
  VkDeviceQueueCreateInfo queueCreateInfo{
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .queueCount = 1,
      .queueFamilyIndex = queueFamilyIndex,
      .pQueuePriorities = priorities,
  };

  VkDeviceCreateInfo deviceCreateInfo{
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = nullptr,
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &queueCreateInfo,
#ifdef VALIDATION_LAYERS
      .enabledLayerCount = layerAndExt.DevLayerCount(),
      .ppEnabledLayerNames = static_cast<const char* const*>(layerAndExt.DevLayerNames()),
      .enabledExtensionCount = layerAndExt.DevExtCount(),
      .ppEnabledExtensionNames = static_cast<const char* const*>(layerAndExt.DevExtNames()),
      .pEnabledFeatures = nullptr
#else
      .enabledLayerCount = 0,
      .ppEnabledLayerNames = nullptr,
      .enabledExtensionCount = static_cast<uint32_t>(device_extensions.size()),
      .ppEnabledExtensionNames = device_extensions.data(),
      .pEnabledFeatures = nullptr,
#endif
  };

  CALL_VK(vkCreateDevice(device.gpuDevice_, &deviceCreateInfo, nullptr, &device.device_));
  vkGetDeviceQueue(device.device_, device.queueFamilyIndex_, 0, &device.queue_);
}

void CreateSwapChain(void) {
  LOGI("->createSwapChain");
  memset(&swapchain, 0, sizeof(swapchain));

  // **********************************************************
  // Get the surface capabilities because:
  //   - It contains the minimal and max length of the chain, we will need it
  //   - It's necessary to query the supported surface format (R8G8B8A8 for
  //   instance ...)
  VkSurfaceCapabilitiesKHR surfaceCapabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.gpuDevice_, device.surface_,
                                            &surfaceCapabilities);
  // Query the list of supported surface format and choose one we like
  uint32_t formatCount = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device.gpuDevice_, device.surface_,
                                       &formatCount, nullptr);
  VkSurfaceFormatKHR* formats = new VkSurfaceFormatKHR[formatCount];
  vkGetPhysicalDeviceSurfaceFormatsKHR(device.gpuDevice_, device.surface_,
                                       &formatCount, formats);
  LOGI("Got %d formats", formatCount);

  uint32_t chosenFormat;
  for (chosenFormat = 0; chosenFormat < formatCount; chosenFormat++) {
    if (formats[chosenFormat].format == VK_FORMAT_R8G8B8A8_UNORM) break;
  }
  assert(chosenFormat < formatCount);

  swapchain.displaySize_ = surfaceCapabilities.currentExtent;
  swapchain.displayFormat_ = formats[chosenFormat].format;

  // **********************************************************
  // Create a swap chain (here we choose the minimum available number of surface
  // in the chain)
  VkSwapchainCreateInfoKHR swapchainCreateInfo{
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .pNext = nullptr,
      .surface = device.surface_,
      .minImageCount = surfaceCapabilities.minImageCount,
      .imageFormat = formats[chosenFormat].format,
      .imageColorSpace = formats[chosenFormat].colorSpace,
      .imageExtent = surfaceCapabilities.currentExtent,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
      .imageArrayLayers = 1,
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 1,
      .pQueueFamilyIndices = &device.queueFamilyIndex_,
      .presentMode = VK_PRESENT_MODE_FIFO_KHR,
      .oldSwapchain = VK_NULL_HANDLE,
      .clipped = VK_FALSE,
  };
  CALL_VK(vkCreateSwapchainKHR(device.device_, &swapchainCreateInfo, nullptr, &swapchain.swapchain_));

  // Get the length of the created swap chain
  CALL_VK(vkGetSwapchainImagesKHR(device.device_, swapchain.swapchain_, &swapchain.swapchainLength_, nullptr));

  // query display attachment to swapchain
  uint32_t SwapchainImagesCount = 0;
  CALL_VK(vkGetSwapchainImagesKHR(device.device_, swapchain.swapchain_, &SwapchainImagesCount, nullptr));
  swapchain.displayImages_.resize(SwapchainImagesCount);
  CALL_VK(vkGetSwapchainImagesKHR(device.device_, swapchain.swapchain_, &SwapchainImagesCount, swapchain.displayImages_.data()));

  // create image view for each swapchain image
  swapchain.displayViews_.resize(SwapchainImagesCount);
  for (uint32_t i = 0; i < SwapchainImagesCount; i++) {
    VkImageViewCreateInfo viewCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .image = swapchain.displayImages_[i],
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = swapchain.displayFormat_,
        .components =
            {
                .r = VK_COMPONENT_SWIZZLE_R,
                .g = VK_COMPONENT_SWIZZLE_G,
                .b = VK_COMPONENT_SWIZZLE_B,
                .a = VK_COMPONENT_SWIZZLE_A,
            },
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        .flags = 0,
    };
    CALL_VK(vkCreateImageView(device.device_, &viewCreateInfo, nullptr, &swapchain.displayViews_[i]));
  }

  delete[] formats;
  LOGI("<-createSwapChain");
}

void DeleteSwapChain(void) {
  for (int i = 0; i < swapchain.swapchainLength_; i++) {
    vkDestroyFramebuffer(device.device_, swapchain.framebuffers_[i], nullptr);
    vkDestroyImageView(device.device_, swapchain.displayViews_[i], nullptr);
    vkDestroyImage(device.device_, swapchain.displayImages_[i], nullptr);
  }
  vkDestroySwapchainKHR(device.device_, swapchain.swapchain_, nullptr);
}

void CreateCommandPool(void) {
  VkCommandPoolCreateInfo cmdPoolCreateInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .pNext = nullptr,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = device.queueFamilyIndex_,
  };
  CALL_VK(vkCreateCommandPool(device.device_, &cmdPoolCreateInfo, nullptr, &render.cmdPool_));
}

void CreateCommandBuffers(void) {

  render.cmdBuffer_.resize(swapchain.swapchainLength_);

  VkCommandBufferAllocateInfo cmdBufferCreateInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = nullptr,
      .commandPool = render.cmdPool_,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = static_cast<uint32_t>(render.cmdBuffer_.size()),
  };
  CALL_VK(vkAllocateCommandBuffers(device.device_, &cmdBufferCreateInfo, render.cmdBuffer_.data()));

}

void CreateDepthStencil(void) {
  VkBool32 validDepthFormat = getSupportedDepthFormat(device.gpuDevice_, &depthStencil.format);
  assert(validDepthFormat);

  VkImageCreateInfo image = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = NULL,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = depthStencil.format,
      .extent = {
          swapchain.displaySize_.width,
          swapchain.displaySize_.height,
          1 },
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
      .flags = 0,
  };


  VkMemoryAllocateInfo memAllocInfo = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = NULL,
      .allocationSize = 0,
      .memoryTypeIndex = 0,
  };

  VkImageViewCreateInfo depthStencilView = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext = NULL,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = depthStencil.format,
      .flags = 0,
      .subresourceRange = {},
      .subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
      .subresourceRange.baseMipLevel = 0,
      .subresourceRange.levelCount = 1,
      .subresourceRange.baseArrayLayer = 0,
      .subresourceRange.layerCount = 1,
      .components.r = VK_COMPONENT_SWIZZLE_R,
      .components.g = VK_COMPONENT_SWIZZLE_G,
      .components.b = VK_COMPONENT_SWIZZLE_B,
      .components.a = VK_COMPONENT_SWIZZLE_A,
  };

  VkMemoryRequirements memReqs;

  CALL_VK(vkCreateImage(device.device_, &image, nullptr, &depthStencil.image));
  vkGetImageMemoryRequirements(device.device_, depthStencil.image, &memReqs);
  memAllocInfo.allocationSize = memReqs.size;
  assert(MapMemoryTypeToIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                              &memAllocInfo.memoryTypeIndex));

  CALL_VK(vkAllocateMemory(device.device_, &memAllocInfo, nullptr, &depthStencil.mem));
  CALL_VK(vkBindImageMemory(device.device_, depthStencil.image, depthStencil.mem, 0));

  depthStencilView.image = depthStencil.image;
  CALL_VK(vkCreateImageView(device.device_, &depthStencilView, nullptr, &depthStencil.view));
}

void CreateRenderPass(void) {
  std::array<VkAttachmentDescription, 2> attachments = {};

  // Color attachment
  attachments[0].format = swapchain.displayFormat_;
  attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
  attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  // Depth attachment
  attachments[1].format = depthStencil.format;
  attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
  attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference colorReference = {
      .attachment = 0,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  };

  VkAttachmentReference depthReference = {
      .attachment = 1,
      .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  };

  VkSubpassDescription subpassDescription{
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .flags = 0,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colorReference,
      .pDepthStencilAttachment = &depthReference,
      .inputAttachmentCount = 0,
      .pInputAttachments = nullptr,
      .pResolveAttachments = nullptr,
      .preserveAttachmentCount = 0,
      .pPreserveAttachments = nullptr,
  };

  // Subpass dependencies for layout transitions
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

  VkRenderPassCreateInfo renderPassCreateInfo{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .pNext = nullptr,
      .attachmentCount = static_cast<uint32_t>(attachments.size()),
      .pAttachments = attachments.data(),
      .subpassCount = 1,
      .pSubpasses = &subpassDescription,
      .dependencyCount = static_cast<uint32_t>(dependencies.size()),
      .pDependencies = dependencies.data(),
  };

  CALL_VK(vkCreateRenderPass(device.device_, &renderPassCreateInfo, nullptr, &render.renderPass_));
}

void CreateFrameBuffers(void) {

  VkImageView attachments[2];
  // Depth/Stencil attachment is the same for all frame buffers
  attachments[1] = depthStencil.view;

  VkFramebufferCreateInfo fbCreateInfo{
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .pNext = nullptr,
      .renderPass = render.renderPass_,
      .attachmentCount = 2,
      .pAttachments = attachments,
      .width = static_cast<uint32_t>(swapchain.displaySize_.width),
      .height = static_cast<uint32_t>(swapchain.displaySize_.height),
      .layers = 1,
  };

  // create a framebuffer from each swapchain image
  swapchain.framebuffers_.resize(swapchain.swapchainLength_);

  for (uint32_t i = 0; i < swapchain.swapchainLength_; i++) {
    attachments[0] = swapchain.displayViews_[i];
    CALL_VK(vkCreateFramebuffer(device.device_, &fbCreateInfo, nullptr, &swapchain.framebuffers_[i]));
  }
}

void CreateUniformBuffer(void) {

  VkBufferCreateInfo createBufferInfo{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = nullptr,
      .size = sizeof(uboVS),
      .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
      .flags = 0,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .pQueueFamilyIndices = &device.queueFamilyIndex_,
      .queueFamilyIndexCount = 1,
  };
  CALL_VK(vkCreateBuffer(device.device_, &createBufferInfo, nullptr, &uniformBuffer.buffer));

  VkMemoryRequirements memReq;
  vkGetBufferMemoryRequirements(device.device_, uniformBuffer.buffer, &memReq);

  VkMemoryAllocateInfo memAllocInfo{
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = nullptr,
      .allocationSize = memReq.size,
      .memoryTypeIndex = 0,  // Memory type assigned in the next step
  };

  // Assign the proper memory type for that buffer
  memAllocInfo.allocationSize = memReq.size;
  MapMemoryTypeToIndex(memReq.memoryTypeBits,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       &memAllocInfo.memoryTypeIndex);

  CALL_VK(vkAllocateMemory(device.device_, &memAllocInfo, nullptr, &uniformBuffer.memory));

  CALL_VK(vkBindBufferMemory(device.device_, uniformBuffer.buffer, uniformBuffer.memory, 0));

//  uniformBuffer.alignment = memReq.alignment;
//  uniformBuffer.size = memAllocInfo.allocationSize;
  uniformBuffer.descriptor.offset = 0;
  uniformBuffer.descriptor.range = sizeof(uboVS);
  uniformBuffer.descriptor.buffer = uniformBuffer.buffer;

  updateUniformBuffers();
 }

void CreateDescriptorSetLayout(void) {
  std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;

  VkDescriptorSetLayoutBinding VertexSetLayoutBinding {
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
      .pImmutableSamplers = nullptr
  };

  setLayoutBindings.push_back(VertexSetLayoutBinding);

  VkDescriptorSetLayoutCreateInfo descriptorLayout{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = static_cast<uint32_t>(setLayoutBindings.size()),
      .pBindings = setLayoutBindings.data(),
  };

  CALL_VK(vkCreateDescriptorSetLayout(device.device_, &descriptorLayout, nullptr, &descriptorSetLayout));

}

void CreatePipelineLayout(void) {

  VkPipelineCacheCreateInfo pipelineCacheInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
      .pNext = nullptr,
      .initialDataSize = 0,
      .pInitialData = nullptr,
      .flags = 0,  // reserved, must be 0
  };
  CALL_VK(vkCreatePipelineCache(device.device_, &pipelineCacheInfo, nullptr, &pipelineCache));

  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .pNext = nullptr,
      .setLayoutCount = 1,
      .pSetLayouts = &descriptorSetLayout,
      .pushConstantRangeCount = 0,
      .pPushConstantRanges = nullptr,
  };
  CALL_VK(vkCreatePipelineLayout(device.device_, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

}

enum ShaderType { VERTEX_SHADER, FRAGMENT_SHADER };
VkResult LoadShaderFromFile(const char* filePath, VkShaderModule* shaderOut, ShaderType type) {
  // Read the file
  assert(androidAppCtx);
  AAsset* file = AAssetManager_open(androidAppCtx->activity->assetManager, filePath, AASSET_MODE_BUFFER);
  size_t fileLength = AAsset_getLength(file);
  assert(fileLength > 0);

  char* fileContent = new char[fileLength];

  AAsset_read(file, fileContent, fileLength);
  AAsset_close(file);

  VkShaderModuleCreateInfo shaderModuleCreateInfo{
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .pNext = nullptr,
      .codeSize = fileLength,
      .pCode = (const uint32_t*)fileContent,
      .flags = 0,
  };
  VkResult result = vkCreateShaderModule( device.device_, &shaderModuleCreateInfo, nullptr, shaderOut);
  assert(result == VK_SUCCESS);

  delete[] fileContent;

  return result;
}

void CreateGraphicsPipeline(void) {

  // Specify input assembler state
  VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE,
  };

  VkPipelineRasterizationStateCreateInfo rasterInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_NONE,
      .frontFace = VK_FRONT_FACE_CLOCKWISE,
      .depthBiasEnable = VK_FALSE,
      .lineWidth = 1.0f,
  };

  VkPipelineColorBlendAttachmentState attachmentStates{
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
      .blendEnable = VK_FALSE,
  };

  VkPipelineColorBlendStateCreateInfo colorBlendInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .pNext = nullptr,
      .attachmentCount = 1,
      .pAttachments = &attachmentStates,
      .flags = 0,
  };
  VkPipelineDepthStencilStateCreateInfo depthStencilState {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .depthTestEnable = VK_TRUE,
      .depthWriteEnable = VK_TRUE,
      .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
      .depthBoundsTestEnable = VK_FALSE,
      .stencilTestEnable = VK_FALSE,
  };

  VkPipelineViewportStateCreateInfo viewportInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .pNext = nullptr,
      .viewportCount = 1,
      .scissorCount = 1,
  };

  VkPipelineMultisampleStateCreateInfo multisampleInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .pSampleMask = nullptr,
  };

  std::vector<VkDynamicState> dynamicStateEnables = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
  };

  VkPipelineDynamicStateCreateInfo dynamicStateInfo {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .flags = 0,
      .pNext = nullptr,
      .dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size()),
      .pDynamicStates = dynamicStateEnables.data()};

  VkShaderModule vertexShader, fragmentShader;
  LoadShaderFromFile("shaders/heart.vert.spv", &vertexShader, VERTEX_SHADER);
  LoadShaderFromFile("shaders/heart.frag.spv", &fragmentShader, FRAGMENT_SHADER);

  // Specify vertex and fragment shader stages
  VkPipelineShaderStageCreateInfo shaderStages[2]{
      {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .pNext = nullptr,
          .stage = VK_SHADER_STAGE_VERTEX_BIT,
          .module = vertexShader,
          .pSpecializationInfo = nullptr,
          .flags = 0,
          .pName = "main",
      },
      {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .pNext = nullptr,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
          .module = fragmentShader,
          .pSpecializationInfo = nullptr,
          .flags = 0,
          .pName = "main",
      }};

  // Specify vertex input state
  VkVertexInputBindingDescription vertexInputBinding{
      .binding = 0,
      .stride = sizeof(Vertex),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  };

  // Inpute attribute bindings describe shader attribute locations and memory layouts
  std::array<VkVertexInputAttributeDescription, 2> vertexInputAttributs;
  // Attribute location 0: Position
  vertexInputAttributs[0].binding = 0;
  vertexInputAttributs[0].location = 0;
  // Position attribute is three 32 bit signed (SFLOAT) floats (R32 G32 B32)
  vertexInputAttributs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
  vertexInputAttributs[0].offset = offsetof(Vertex, pos);
  // Attribute location 1: Color
  vertexInputAttributs[1].binding = 0;
  vertexInputAttributs[1].location = 1;
  // Color attribute is three 32 bit signed (SFLOAT) floats (R32 G32 B32)
  vertexInputAttributs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
  vertexInputAttributs[1].offset = offsetof(Vertex, color);

  VkPipelineVertexInputStateCreateInfo vertexInputInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .pNext = nullptr,
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &vertexInputBinding,
      .vertexAttributeDescriptionCount = 2,
      .pVertexAttributeDescriptions = vertexInputAttributs.data(),
  };

  // Create the pipeline
  VkGraphicsPipelineCreateInfo pipelineCreateInfo{
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .stageCount = 2,
      .pStages = shaderStages,
      .pVertexInputState = &vertexInputInfo,
      .pInputAssemblyState = &inputAssemblyInfo,
      .pViewportState = &viewportInfo,
      .pRasterizationState = &rasterInfo,
      .pMultisampleState = &multisampleInfo,
      .pDepthStencilState = &depthStencilState,
      .pColorBlendState = &colorBlendInfo,
      .pDynamicState = &dynamicStateInfo,
      .layout = pipelineLayout,
      .renderPass = render.renderPass_,
      .subpass = 0,
  };

  CALL_VK(vkCreateGraphicsPipelines(device.device_, pipelineCache, 1,
                                    &pipelineCreateInfo, nullptr, &gfxPipeline));

  // We don't need the shaders anymore, we can release their memory
  vkDestroyShaderModule(device.device_, vertexShader, nullptr);
  vkDestroyShaderModule(device.device_, fragmentShader, nullptr);

}

void DeleteGraphicsPipeline(void) {
  if (gfxPipeline == VK_NULL_HANDLE) return;
  vkDestroyPipeline(device.device_, gfxPipeline, nullptr);
  vkDestroyPipelineCache(device.device_, pipelineCache, nullptr);
  vkDestroyPipelineLayout(device.device_, pipelineLayout, nullptr);
}

void CreateSyncronization(void) {

  VkSemaphoreCreateInfo semaphoreCreateInfo{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
  };
  CALL_VK(vkCreateSemaphore(device.device_, &semaphoreCreateInfo, nullptr, &render.semaphore_));

  CALL_VK(vkCreateSemaphore(device.device_, &semaphoreCreateInfo, nullptr, &swapchain.semaphore_));
}

void CreateDescriptorPool(void) {
  std::vector<VkDescriptorPoolSize> poolSizes;

  VkDescriptorPoolSize descriptorPoolSize{
      .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
  };
  poolSizes.push_back(descriptorPoolSize);

  VkDescriptorPoolCreateInfo descriptorPoolInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
      .pPoolSizes = poolSizes.data(),
      .maxSets = 1,
  };

  CALL_VK(vkCreateDescriptorPool(device.device_, &descriptorPoolInfo, nullptr, &descriptorPool));
}

void CreateDescriptorSet(void) {

  VkDescriptorSetAllocateInfo allocInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = descriptorPool,
      .pSetLayouts = &descriptorSetLayout,
      .descriptorSetCount = 1,
  };

  CALL_VK(vkAllocateDescriptorSets(device.device_, &allocInfo, &descriptorSet));

  std::vector<VkWriteDescriptorSet> writeDescriptorSets;

  VkWriteDescriptorSet writeDescriptorSet{
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = descriptorSet,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .dstBinding = 0,
      .pBufferInfo = &uniformBuffer.descriptor,
      .descriptorCount = 1,
  };

  writeDescriptorSets.push_back(writeDescriptorSet);

  vkUpdateDescriptorSets(device.device_, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);
}

void BuildCommandBuffers(void) {

  // Command for all buffers
  VkCommandBufferBeginInfo cmdBufferBeginInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext = nullptr,
      .flags = 0,
      .pInheritanceInfo = nullptr,
  };

  VkClearValue clearValues[2];
  clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
  clearValues[1].depthStencil = { 1.0f, 0 };

  VkRenderPassBeginInfo renderPassBeginInfo{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .pNext = nullptr,
      .renderPass = render.renderPass_,
      .renderArea.offset.x = 0,
      .renderArea.offset.y = 0,
      .renderArea.extent = swapchain.displaySize_,
      .clearValueCount = 2,
      .pClearValues = clearValues};

  for (int32_t i = 0; i < render.cmdBuffer_.size();  i++) {

    renderPassBeginInfo.framebuffer = swapchain.framebuffers_[i];

    // We start by creating and declare the "beginning" our command buffer
    CALL_VK(vkBeginCommandBuffer(render.cmdBuffer_[i], &cmdBufferBeginInfo));


    // Now we start a renderpass. Any draw command has to be recorded in a
    // renderpass

    vkCmdBeginRenderPass(render.cmdBuffer_[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewports{
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
        .x = 0,
        .y = 0,
        .width = (float)swapchain.displaySize_.width,
        .height = (float)swapchain.displaySize_.height,
    };
    vkCmdSetViewport(render.cmdBuffer_[i], 0, 1, &viewports);

    VkRect2D scissor = {
        .extent = swapchain.displaySize_,
        .offset.x = 0,
        .offset.y = 0,
    };
    vkCmdSetScissor(render.cmdBuffer_[i], 0, 1, &scissor);

    vkCmdBindDescriptorSets(render.cmdBuffer_[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
                            0, 1, &descriptorSet, 0, nullptr);

    vkCmdBindPipeline(render.cmdBuffer_[i], VK_PIPELINE_BIND_POINT_GRAPHICS, gfxPipeline);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(render.cmdBuffer_[i], 0, 1,  &vertices.buffer, &offset);

    // Bind triangle index buffer
    vkCmdBindIndexBuffer(render.cmdBuffer_[i], indices.buffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(render.cmdBuffer_[i], indices.count, 1, 0, 0, 1);

    // transition the display image to color attachment layout
//    SetImageLayout(render.cmdBuffer_[i],
//                   swapchain.displayImages_[i],
//                   VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
//                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
//                   VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
//                   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
//



    vkCmdEndRenderPass(render.cmdBuffer_[i]);
//    // transition back to swapchain image to PRESENT_SRC_KHR
//    SetImageLayout(render.cmdBuffer_[i],
//                   swapchain.displayImages_[i],
//                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
//                   VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
//                   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
//                   VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    CALL_VK(vkEndCommandBuffer(render.cmdBuffer_[i]));
  }

}

// Creates generic buffer
VkResult CreateBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags,
                      VkDeviceSize size, VkBuffer *buffer, VkDeviceMemory *memory,
                      void *data = nullptr) {

  // Create a vertex buffer
  VkBufferCreateInfo bufferCreateInfo{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = nullptr,
      .size = size,
      .usage = usageFlags,
      .flags = 0,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .pQueueFamilyIndices = &device.queueFamilyIndex_,
      .queueFamilyIndexCount = 1,
  };

  CALL_VK(vkCreateBuffer(device.device_, &bufferCreateInfo, nullptr, buffer));

  VkMemoryRequirements memReq;
  vkGetBufferMemoryRequirements(device.device_, *buffer, &memReq);

  VkMemoryAllocateInfo memAllocInfo{
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = nullptr,
      .allocationSize = memReq.size,
      .memoryTypeIndex = 0,  // Memory type assigned in the next step
  };

  // Assign the proper memory type for that buffer
  assert(MapMemoryTypeToIndex(memReq.memoryTypeBits, memoryPropertyFlags, &memAllocInfo.memoryTypeIndex));

  // Allocate memory for the buffer
  CALL_VK(vkAllocateMemory(device.device_, &memAllocInfo, nullptr, memory));

  // If a pointer to the buffer data has been passed, map the buffer and copy over the data
  if (data != nullptr)
  {
    void *mapped;
    CALL_VK(vkMapMemory(device.device_, *memory, 0, size, 0, &mapped));
    memcpy(mapped, data, size);

    // If host coherency hasn't been requested, do a manual flush to make writes visible
    if ((memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0) {
      VkMappedMemoryRange mappedMemoryRange {
          .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
          .memory = *memory,
          .offset = 0,
          .size = size
      };
      vkFlushMappedMemoryRanges(device.device_, 1, &mappedMemoryRange);
    }

    vkUnmapMemory(device.device_, *memory);
  }

  // Attach the memory to the buffer object
  CALL_VK(vkBindBufferMemory(device.device_, *buffer, *memory, 0));

  return VK_SUCCESS;
}

void LoadModel(const char* filePath, float scale) {

  const aiScene* scene;
  Assimp::Importer Importer;

  // Flags for loading the mesh
  static const int assimpFlags = aiProcess_FlipWindingOrder | aiProcess_Triangulate | aiProcess_PreTransformVertices;

  assert(androidAppCtx);
  AAsset* file = AAssetManager_open(androidAppCtx->activity->assetManager, filePath, AASSET_MODE_STREAMING);

  size_t fileLength = AAsset_getLength(file);
  assert(fileLength > 0);

  char* meshData = new char[fileLength];

  AAsset_read(file, meshData, fileLength);
  AAsset_close(file);

  scene = Importer.ReadFileFromMemory(meshData, fileLength, assimpFlags);

  delete[] meshData;

  // Generate vertex buffer from ASSIMP scene data
  std::vector<Vertex> vertexBuffer;

  // Iterate through all meshes in the file and extract the vertex components
  for (uint32_t m = 0; m < scene->mNumMeshes; m++)
  {
    for (uint32_t v = 0; v < scene->mMeshes[m]->mNumVertices; v++)
    {
      Vertex vertex;

      // Use glm make_* functions to convert ASSIMP vectors to glm vectors
      vertex.pos = glm::make_vec3(&scene->mMeshes[m]->mVertices[v].x) * scale;
      vertex.normal = glm::make_vec3(&scene->mMeshes[m]->mNormals[v].x);
      // Texture coordinates and colors may have multiple channels, we only use the first [0] one
      vertex.uv = glm::make_vec2(&scene->mMeshes[m]->mTextureCoords[0][v].x);
      // Mesh may not have vertex colors
      vertex.color = (scene->mMeshes[m]->HasVertexColors(0)) ? glm::make_vec3(&scene->mMeshes[m]->mColors[0][v].r) : glm::vec3(1.0f);

      // Vulkan uses a right-handed NDC (contrary to OpenGL), so simply flip Y-Axis
      vertex.pos.y *= -1.0f;

      vertexBuffer.push_back(vertex);
    }
  }
  size_t vertexBufferSize = vertexBuffer.size() * sizeof(Vertex);

  // Generate index buffer from ASSIMP scene data
  std::vector<uint32_t> indexBuffer;
  for (uint32_t m = 0; m < scene->mNumMeshes; m++)
  {
    uint32_t indexBase = static_cast<uint32_t>(indexBuffer.size());
    for (uint32_t f = 0; f < scene->mMeshes[m]->mNumFaces; f++)
    {
      // We assume that all faces are triangulated
      for (uint32_t i = 0; i < 3; i++)
      {
        indexBuffer.push_back(scene->mMeshes[m]->mFaces[f].mIndices[i] + indexBase);
      }
    }
  }
  size_t indexBufferSize = indexBuffer.size() * sizeof(uint32_t);
//  model.indices.count = static_cast<uint32_t>(indexBuffer.size());

  // Vertex buffer
  CALL_VK(CreateBuffer(
      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      vertexBufferSize,
      &model.vertices.buffer,
      &model.vertices.memory,
      vertexBuffer.data()));

  // Index buffer
  CALL_VK(CreateBuffer(
      VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      indexBufferSize,
      &model.indices.buffer,
      &model.indices.memory,
      indexBuffer.data()));
}

VkResult LoadTextureFromFile(const char* filePath, struct Texture* texure) {

  // Check for optimal tiling supportability
  VkFormatProperties props;
  vkGetPhysicalDeviceFormatProperties(device.gpuDevice_, texure->format, &props);
  assert(props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);

  // Read the file:
  AAsset* file = AAssetManager_open(androidAppCtx->activity->assetManager, filePath,
                                    AASSET_MODE_BUFFER);
  size_t fileLength = AAsset_getLength(file);
  stbi_uc* fileContent = new unsigned char[fileLength];
  AAsset_read(file, fileContent, fileLength);
  AAsset_close(file);

  // Get image data from stb
  uint32_t imgWidth, imgHeight, n;
  unsigned char* imageData = stbi_load_from_memory(
      fileContent, fileLength, reinterpret_cast<int*>(&imgWidth),
      reinterpret_cast<int*>(&imgHeight), reinterpret_cast<int*>(&n), 4);
  assert(n == 4);
  texure->width = imgWidth;
  texure->height = imgHeight;
  size_t texureSize = imgWidth * imgHeight * 4;

  // Create a host-visible staging buffer that contains the raw image data
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingMemory;

  VkBufferCreateInfo bufferCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = texureSize,
      .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };
  CALL_VK(vkCreateBuffer(device.device_, &bufferCreateInfo, nullptr, &stagingBuffer));


  VkMemoryRequirements memReqs;
  vkGetImageMemoryRequirements(device.device_, stagingBuffer, &memReqs);

  VkMemoryAllocateInfo memAllocInfo = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = nullptr,
      .allocationSize = memReqs.size,
      .memoryTypeIndex = 0,
  };
  assert(MapMemoryTypeToIndex(memReqs.memoryTypeBits,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                              &memAllocInfo.memoryTypeIndex));

  CALL_VK(vkAllocateMemory(device.device_, &memAllocInfo, nullptr, &stagingMemory));
  CALL_VK(vkBindImageMemory(device.device_, stagingBuffer, stagingMemory, 0));


  // Copy texture data into staging buffer
  uint8_t *data;
  CALL_VK(vkMapMemory(device.device_, stagingMemory, 0, memReqs.size, 0, (void **)&data));
  memcpy(data, (void*) imageData, texureSize);
  vkUnmapMemory(device.device_, stagingMemory);

  // Setup buffer copy regions for each mip level, but only 1 for now
  std::vector<VkBufferImageCopy> bufferCopyRegions;
  VkBufferImageCopy bufferCopyRegion = {
      .imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .imageSubresource.mipLevel = 0,
      .imageSubresource.baseArrayLayer = 0,
      .imageSubresource.layerCount = 1,
      .imageExtent.width = imgWidth,
      .imageExtent.height = imgHeight,
      .imageExtent.depth = 1,
      .bufferOffset = 0,
  };
  bufferCopyRegions.push_back(bufferCopyRegion);

  // Create optimal tiled target image
  VkImageCreateInfo imageCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = nullptr,
      .imageType = texure->type,
      .format = texure->format,
      .extent = {texure->width, texure->height, 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 1,
      .pQueueFamilyIndices = &device.queueFamilyIndex_,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .flags = 0,
  };

  CALL_VK(vkCreateImage(device.device_, &imageCreateInfo, nullptr, &texure->image));

  vkGetImageMemoryRequirements(device.device_, texure->image, &memReqs);
  memAllocInfo.allocationSize = memReqs.size;
  assert(MapMemoryTypeToIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                              &memAllocInfo.memoryTypeIndex));

  CALL_VK(vkAllocateMemory(device.device_, &memAllocInfo, nullptr, &texure->memory));
  CALL_VK(vkBindImageMemory(device.device_, texure->image, texure->memory, 0));

  // Create copy commandbuffer
  VkCommandPoolCreateInfo cmdPoolCreateInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .pNext = nullptr,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = device.queueFamilyIndex_,
  };

  VkCommandPool cmdPool;
  CALL_VK(vkCreateCommandPool(device.device_, &cmdPoolCreateInfo, nullptr,
                              &cmdPool));

  VkCommandBuffer copyCmd;
  const VkCommandBufferAllocateInfo cmd = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = nullptr,
      .commandPool = cmdPool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };

  CALL_VK(vkAllocateCommandBuffers(device.device_, &cmd, &copyCmd));
  VkCommandBufferBeginInfo cmdBufInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext = nullptr,
      .flags = 0,
      .pInheritanceInfo = nullptr};

  CALL_VK(vkBeginCommandBuffer(copyCmd, &cmdBufInfo));


  // transitions image out of UNDEFINED type
  SetImageLayout(copyCmd, texure->image,
                 VK_IMAGE_LAYOUT_UNDEFINED,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // VK_PIPELINE_STAGE_HOST_BIT
                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT); // VK_PIPELINE_STAGE_TRANSFER_BIT

  // Copy the layers and mip levels from the staging buffer to the optimal tiled image
  vkCmdCopyBufferToImage(
      copyCmd,
      stagingBuffer,
      texure->image,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      static_cast<uint32_t>(bufferCopyRegions.size()),
      bufferCopyRegions.data());

  // Change texture image layout to shader read after all faces have been copied
  texure->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  SetImageLayout(copyCmd, texure->image,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      texure->layout,
      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // VK_PIPELINE_STAGE_TRANSFER_BIT
      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);// VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT


  CALL_VK(vkEndCommandBuffer(copyCmd));

  VkFenceCreateInfo fenceInfo = {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
  };
  VkFence fence;
  CALL_VK(vkCreateFence(device.device_, &fenceInfo, nullptr, &fence));

  VkSubmitInfo submitInfo = {
      .pNext = nullptr,
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount = 0,
      .pWaitSemaphores = nullptr,
      .pWaitDstStageMask = nullptr,
      .commandBufferCount = 1,
      .pCommandBuffers = &copyCmd,
      .signalSemaphoreCount = 0,
      .pSignalSemaphores = nullptr,
  };
  CALL_VK(vkQueueSubmit(device.queue_, 1, &submitInfo, fence));
  CALL_VK(vkWaitForFences(device.device_, 1, &fence, VK_TRUE, 100000000));
  vkDestroyFence(device.device_, fence, nullptr);

  vkFreeCommandBuffers(device.device_, cmdPool, 1, &copyCmd);
  vkDestroyCommandPool(device.device_, cmdPool, nullptr);

  vkDestroyImage(device.device_, stagingBuffer, nullptr);
  vkFreeMemory(device.device_, stagingMemory, nullptr);

  stbi_image_free(imageData);

  return VK_SUCCESS;
}


// InitVulkan:
//   Initialize Vulkan Context when android application window is created
//   upon return, vulkan is ready to draw frames
bool InitVulkan(android_app* app) {
  androidAppCtx = app;

  if (!InitVulkan()) {
    LOGW("Vulkan is unavailable, install vulkan and re-start");
    return false;
  }

  CreateVulkanDevice(app->window);
  CreateSwapChain();
  CreateCommandPool();
  CreateCommandBuffers();
  CreateDepthStencil();
  CreateRenderPass();
  CreateFrameBuffers();
  LoadModel("models/heart/HeartAnim.fbx", 1.0f);
  LoadTextureFromFile("sample_tex.png", &heartTexture);
  CreateUniformBuffer();
  CreateDescriptorSetLayout();
  CreatePipelineLayout();
  CreateGraphicsPipeline();
  CreateSyncronization();
  CreateDescriptorPool();
  CreateDescriptorSet();



  BuildCommandBuffers();

  viewChanged = false;
  device.initialized_ = true;
  return true;
}

// IsVulkanReady():
//    native app poll to see if we are ready to draw...
bool IsVulkanReady(void) { return device.initialized_; }

void DeleteVulkan(void) {
  vkFreeCommandBuffers(device.device_, render.cmdPool_,
                       render.cmdBuffer_.size(), render.cmdBuffer_.data());

  vkDestroyCommandPool(device.device_, render.cmdPool_, nullptr);
  vkDestroyRenderPass(device.device_, render.renderPass_, nullptr);
  DeleteSwapChain();
  DeleteGraphicsPipeline();
  model.destroy(device.device_);

  vkDestroyDevice(device.device_, nullptr);
  vkDestroyInstance(device.instance_, nullptr);

  device.initialized_ = false;
}

bool VulkanDrawFrame(void) {

  updateUniformBuffers();

  uint32_t nextIndex;
  // Get the framebuffer index we should draw in
  CALL_VK(vkAcquireNextImageKHR(
      device.device_, swapchain.swapchain_, UINT64_MAX, render.semaphore_, VK_NULL_HANDLE, &nextIndex));

  VkPipelineStageFlags waitStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submit_info = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                              .pNext = nullptr,
                              .waitSemaphoreCount = 1,
                              .pWaitSemaphores = &render.semaphore_,
                              .pWaitDstStageMask = &waitStageMask,
                              .commandBufferCount = 1,
                              .pCommandBuffers = &render.cmdBuffer_[nextIndex],
                              .signalSemaphoreCount = 1,
                              .pSignalSemaphores = &swapchain.semaphore_};

  CALL_VK(vkQueueSubmit(device.queue_, 1, &submit_info, VK_NULL_HANDLE));

  CALL_VK(vkQueueWaitIdle(device.queue_));

  VkResult result;
  VkPresentInfoKHR presentInfo{
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .pNext = nullptr,
      .swapchainCount = 1,
      .pSwapchains = &swapchain.swapchain_,
      .pImageIndices = &nextIndex,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &swapchain.semaphore_,
      .pResults = &result,
  };
  vkQueuePresentKHR(device.queue_, &presentInfo);
  return true;
}

/*
 * SetImageLayout():
 *    Helper function to transition color buffer layout
 */
void SetImageLayout(VkCommandBuffer cmdBuffer, VkImage image,
                    VkImageLayout oldImageLayout, VkImageLayout newImageLayout,
                    VkPipelineStageFlags srcStages,
                    VkPipelineStageFlags destStages) {
  VkImageMemoryBarrier imageMemoryBarrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .pNext = NULL,
      .srcAccessMask = 0,
      .dstAccessMask = 0,
      .oldLayout = oldImageLayout,
      .newLayout = newImageLayout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange =
          {
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .baseMipLevel = 0,
              .levelCount = 1,
              .baseArrayLayer = 0,
              .layerCount = 1,
          },
  };

  switch (oldImageLayout) {
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      break;

    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      break;

    case VK_IMAGE_LAYOUT_PREINITIALIZED:
      imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
      break;

    default:
      break;
  }

  switch (newImageLayout) {
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      imageMemoryBarrier.dstAccessMask =
          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      break;

    default:
      break;
  }

  vkCmdPipelineBarrier(cmdBuffer, srcStages, destStages, 0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);
}
