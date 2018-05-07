#include <android_native_app_glue.h>

#include <cassert>
#include <cstring>
#include <vector>
#include <array>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <gli/gli.hpp>

#include "VulkanUtil.h"
#include "ModelLoader.h"
#include "ValidationLayers.h"

using namespace navs;

// Android Native App pointer...
android_app* androidAppCtx = nullptr;

struct VulkanDeviceInfo {
  bool initialized_;

  VkInstance instance_;
  VkPhysicalDevice physical_;
  VkDevice logic_;
  uint32_t queueFamilyIndex_;

  VkSurfaceKHR surface_;
  VkQueue queue_;
} device;

struct VulkanSwapchainInfo {
  VkSwapchainKHR cmdBuffer;
  uint32_t length;

  VkExtent2D displaySize;
  VkFormat displayFormat;
  VkSemaphore semaphore;

  // array of frame buffers and views
  std::vector<VkImage> displayImages;
  std::vector<VkImageView> displayViews;
  std::vector<VkFramebuffer> framebuffers;
};
VulkanSwapchainInfo swapchain;

struct VulkanRenderInfo {
  VkRenderPass renderPass;
  VkCommandPool cmdPool;
  std::vector<VkCommandBuffer> cmdBuffe;
  VkSemaphore semaphore;
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
  glm::mat4 MVP;
  glm::mat4 modelMatrix;
  glm::mat4 normal;
  glm::vec4 lightPos = glm::vec4(0.0f, 1.0f, 4.0f, 1.0f);
} uboVS;

float zoom = -6.0f;
glm::vec3 rotation = glm::vec3(0.0f, 0.0f, 0.0f);

struct Vertex {
  glm::vec3 pos;
  glm::vec3 normal;
  glm::vec2 uv;
  glm::vec3 color;
};

struct {
  VkPipelineVertexInputStateCreateInfo inputState;
  std::vector<VkVertexInputBindingDescription> bindingDescriptions;
  std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
} vertices;

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

struct Texture heartMainTexture {
    .type = VK_IMAGE_TYPE_2D,
    .format = VK_FORMAT_ASTC_8x8_UNORM_BLOCK,
};

struct Texture heartNormalTexture {
    .type = VK_IMAGE_TYPE_2D,
    .format = VK_FORMAT_ASTC_8x8_UNORM_BLOCK,
};

ModelLoader* modelLoader;
struct ModelLoader::Model heartModel;

/*
 * SetImageLayout():
 *    Helper function to transition color buffer layout
 */
void SetImageLayout(VkCommandBuffer cmdBuffer, VkImage image, uint32_t mipLevels,
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
              .levelCount = mipLevels,
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

void updateUniformBuffers(void) {

  uboVS.modelMatrix = glm::mat4(1.0f);
  uboVS.modelMatrix = glm::rotate(uboVS.modelMatrix, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
  uboVS.modelMatrix = glm::rotate(uboVS.modelMatrix, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
  uboVS.modelMatrix = glm::rotate(uboVS.modelMatrix, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

  glm::mat4 projectionMatrix = glm::perspective(glm::radians(60.0f),
                                                (float)(swapchain.displaySize.width) / (float)swapchain.displaySize.height,
                                                0.01f, 256.0f);

  glm::mat4 viewMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, zoom));

  uboVS.MVP = projectionMatrix * viewMatrix * uboVS.modelMatrix;

  uboVS.normal = glm::inverseTranspose(uboVS.modelMatrix);

  uint8_t *pData;
  CALL_VK(vkMapMemory(device.logic_, uniformBuffer.memory, 0, sizeof(uboVS), 0, (void **)&pData));
  memcpy(pData, &uboVS, sizeof(uboVS));
  vkUnmapMemory(device.logic_, uniformBuffer.memory);
}

VkBool32 getSupportedDepthFormat(VkFormat *depthFormat)
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
    vkGetPhysicalDeviceFormatProperties(device.physical_, format, &formatProps);
    // Format must support depth stencil attachment for optimal tiling
    if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
    {
      *depthFormat = format;
      return true;
    }
  }

  return false;
}


VkResult LoadTextureFromFile(const char* filePath, struct Texture* texture) {

  // Check for optimal tiling supportability
  VkFormatProperties props;
  vkGetPhysicalDeviceFormatProperties(device.physical_, texture->format, &props);
  assert(props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);

  // Read the file:
  AAsset* file = AAssetManager_open(androidAppCtx->activity->assetManager, filePath,
                                    AASSET_MODE_BUFFER);
  size_t fileLength = AAsset_getLength(file);
  char* fileContent = new char[fileLength];
  AAsset_read(file, fileContent, fileLength);
  AAsset_close(file);

  gli::texture2d imageData(gli::load((const char*)fileContent, fileLength));
  assert(!imageData.empty());

  texture->width = static_cast<uint32_t>(imageData[0].extent().x);
  texture->height = static_cast<uint32_t>(imageData[0].extent().y);
  texture->mipLevels = static_cast<uint32_t>(imageData.levels());

  // Create a host-visible staging buffer that contains the raw image data
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingMemory;

  VkBufferCreateInfo bufferCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = imageData.size(),
      .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };
  CALL_VK(vkCreateBuffer(device.logic_, &bufferCreateInfo, nullptr, &stagingBuffer));


  VkMemoryRequirements memReqs;
  vkGetImageMemoryRequirements(device.logic_, stagingBuffer, &memReqs);

  VkMemoryAllocateInfo memAllocInfo = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = nullptr,
      .allocationSize = memReqs.size,
      .memoryTypeIndex = 0,
  };
  assert(MapMemoryTypeToIndex(device.physical_, memReqs.memoryTypeBits,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                              &memAllocInfo.memoryTypeIndex));

  CALL_VK(vkAllocateMemory(device.logic_, &memAllocInfo, nullptr, &stagingMemory));
  CALL_VK(vkBindBufferMemory(device.logic_, stagingBuffer, stagingMemory, 0));


  // Copy texture data into staging buffer
  uint8_t *data;
  CALL_VK(vkMapMemory(device.logic_, stagingMemory, 0, memReqs.size, 0, (void **)&data));
  memcpy(data, imageData.data(), imageData.size());
  vkUnmapMemory(device.logic_, stagingMemory);

  // Setup buffer copy regions for each mip level, but only 1 for now
  std::vector<VkBufferImageCopy> bufferCopyRegions;
  uint32_t offset = 0;

  for (uint32_t i = 0; i < texture->mipLevels; i++)
  {
    VkBufferImageCopy bufferCopyRegion = {
        .imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .imageSubresource.mipLevel = i,
        .imageSubresource.baseArrayLayer = 0,
        .imageSubresource.layerCount = 1,
        .imageExtent.width = static_cast<uint32_t>(imageData[i].extent().x),
        .imageExtent.height = static_cast<uint32_t>(imageData[i].extent().y),
        .imageExtent.depth = 1,
        .bufferOffset = offset,
    };

    bufferCopyRegions.push_back(bufferCopyRegion);

    offset += static_cast<uint32_t>(imageData[i].size());
  }



  // Create optimal tiled target image
  VkImageCreateInfo imageCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = nullptr,
      .imageType = texture->type,
      .format = texture->format,
      .extent = {texture->width, texture->height, 1},
      .mipLevels = texture->mipLevels,
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

  CALL_VK(vkCreateImage(device.logic_, &imageCreateInfo, nullptr, &texture->image));

  vkGetImageMemoryRequirements(device.logic_, texture->image, &memReqs);
  memAllocInfo.allocationSize = memReqs.size;

  assert(MapMemoryTypeToIndex(device.physical_, memReqs.memoryTypeBits,
                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memAllocInfo.memoryTypeIndex));

  CALL_VK(vkAllocateMemory(device.logic_, &memAllocInfo, nullptr, &texture->memory));
  CALL_VK(vkBindImageMemory(device.logic_, texture->image, texture->memory, 0));

  // Create copy commandbuffer
  VkCommandPoolCreateInfo cmdPoolCreateInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .pNext = nullptr,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = device.queueFamilyIndex_,
  };

  VkCommandPool cmdPool;
  CALL_VK(vkCreateCommandPool(device.logic_, &cmdPoolCreateInfo, nullptr,
                              &cmdPool));

  VkCommandBuffer copyCmd;
  const VkCommandBufferAllocateInfo cmd = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = nullptr,
      .commandPool = cmdPool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };

  CALL_VK(vkAllocateCommandBuffers(device.logic_, &cmd, &copyCmd));
  VkCommandBufferBeginInfo cmdBufInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext = nullptr,
      .flags = 0,
      .pInheritanceInfo = nullptr};

  CALL_VK(vkBeginCommandBuffer(copyCmd, &cmdBufInfo));


  // transitions image out of UNDEFINED type
  SetImageLayout(copyCmd, texture->image, texture->mipLevels,
                 VK_IMAGE_LAYOUT_UNDEFINED,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // VK_PIPELINE_STAGE_HOST_BIT
                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT); // VK_PIPELINE_STAGE_TRANSFER_BIT

  // Copy the layers and mip levels from the staging buffer to the optimal tiled image
  vkCmdCopyBufferToImage(
      copyCmd,
      stagingBuffer,
      texture->image,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      static_cast<uint32_t>(bufferCopyRegions.size()),
      bufferCopyRegions.data());

  // Change texture image layout to shader read after all faces have been copied
  texture->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  SetImageLayout(copyCmd, texture->image, texture->mipLevels,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 texture->layout,
                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // VK_PIPELINE_STAGE_TRANSFER_BIT
                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);// VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT


  CALL_VK(vkEndCommandBuffer(copyCmd));

  VkFenceCreateInfo fenceInfo = {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
  };
  VkFence fence;
  CALL_VK(vkCreateFence(device.logic_, &fenceInfo, nullptr, &fence));

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
  CALL_VK(vkWaitForFences(device.logic_, 1, &fence, VK_TRUE, 100000000));
  vkDestroyFence(device.logic_, fence, nullptr);

  vkFreeCommandBuffers(device.logic_, cmdPool, 1, &copyCmd);
  vkDestroyCommandPool(device.logic_, cmdPool, nullptr);

  vkDestroyImage(device.logic_, stagingBuffer, nullptr);
  vkFreeMemory(device.logic_, stagingMemory, nullptr);

  return VK_SUCCESS;
}

// Create vulkan device
void CreateVulkanDevice(ANativeWindow *platformWindow) {

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
      .ppEnabledExtensionNames = static_cast<const char *const *>(layerAndExt.InstExtNames()),
      .enabledLayerCount = layerAndExt.InstLayerCount(),
      .ppEnabledLayerNames = static_cast<const char *const *>(layerAndExt.InstLayerNames()),
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
  device.physical_ = tmpGpus[0];  // Pick up the first GPU Device

#ifdef VALIDATION_LAYERS
  layerAndExt.InitDevLayersAndExt(device.physical_);
#endif

  // Find a GFX queue family
  uint32_t queueFamilyCount;
  vkGetPhysicalDeviceQueueFamilyProperties(device.physical_, &queueFamilyCount, nullptr);
  assert(queueFamilyCount);
  std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(device.physical_,
                                           &queueFamilyCount,
                                           queueFamilyProperties.data());

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
      .ppEnabledLayerNames = static_cast<const char *const *>(layerAndExt.DevLayerNames()),
      .enabledExtensionCount = layerAndExt.DevExtCount(),
      .ppEnabledExtensionNames = static_cast<const char *const *>(layerAndExt.DevExtNames()),
      .pEnabledFeatures = nullptr
#else
  .enabledLayerCount = 0,
  .ppEnabledLayerNames = nullptr,
  .enabledExtensionCount = static_cast<uint32_t>(device_extensions.size()),
  .ppEnabledExtensionNames = device_extensions.data(),
  .pEnabledFeatures = nullptr,
#endif
  };

  CALL_VK(vkCreateDevice(device.physical_, &deviceCreateInfo, nullptr, &device.logic_));
  vkGetDeviceQueue(device.logic_, device.queueFamilyIndex_, 0, &device.queue_);
}

void CreateSwapChain(void) {
  memset(&swapchain, 0, sizeof(swapchain));

  // **********************************************************
  // Get the surface capabilities because:
  //   - It contains the minimal and max length of the chain, we will need it
  //   - It's necessary to query the supported surface format (R8G8B8A8 for
  //   instance ...)
  VkSurfaceCapabilitiesKHR surfaceCapabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.physical_, device.surface_,
                                            &surfaceCapabilities);
  // Query the list of supported surface format and choose one we like
  uint32_t formatCount = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device.physical_, device.surface_,
                                       &formatCount, nullptr);
  VkSurfaceFormatKHR* formats = new VkSurfaceFormatKHR[formatCount];
  vkGetPhysicalDeviceSurfaceFormatsKHR(device.physical_, device.surface_,
                                       &formatCount, formats);
  assert(formatCount > 0);

  uint32_t chosenFormat;
  for (chosenFormat = 0; chosenFormat < formatCount; chosenFormat++) {
    if (formats[chosenFormat].format == VK_FORMAT_R8G8B8A8_UNORM) break;
  }
  assert(chosenFormat < formatCount);

  swapchain.displaySize = surfaceCapabilities.currentExtent;
  swapchain.displayFormat = formats[chosenFormat].format;

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
  CALL_VK(vkCreateSwapchainKHR(device.logic_, &swapchainCreateInfo, nullptr, &swapchain.cmdBuffer));

  // Get the length of the created swap chain
  CALL_VK(vkGetSwapchainImagesKHR(device.logic_, swapchain.cmdBuffer, &swapchain.length, nullptr));

  // query display attachment to swapchain
  uint32_t SwapchainImagesCount = 0;
  CALL_VK(vkGetSwapchainImagesKHR(device.logic_, swapchain.cmdBuffer, &SwapchainImagesCount, nullptr));
  swapchain.displayImages.resize(SwapchainImagesCount);
  CALL_VK(vkGetSwapchainImagesKHR(device.logic_, swapchain.cmdBuffer, &SwapchainImagesCount, swapchain.displayImages.data()));

  // create image view for each swapchain image
  swapchain.displayViews.resize(SwapchainImagesCount);
  for (uint32_t i = 0; i < SwapchainImagesCount; i++) {
    VkImageViewCreateInfo viewCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .image = swapchain.displayImages[i],
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = swapchain.displayFormat,
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
    CALL_VK(vkCreateImageView(device.logic_, &viewCreateInfo, nullptr, &swapchain.displayViews[i]));
  }

  delete[] formats;
}

void DeleteSwapChain(void) {
  for (int i = 0; i < swapchain.length; i++) {
    vkDestroyFramebuffer(device.logic_, swapchain.framebuffers[i], nullptr);
    vkDestroyImageView(device.logic_, swapchain.displayViews[i], nullptr);
    vkDestroyImage(device.logic_, swapchain.displayImages[i], nullptr);
  }
  vkDestroySwapchainKHR(device.logic_, swapchain.cmdBuffer, nullptr);
}

void CreateCommandPool(void) {
  VkCommandPoolCreateInfo cmdPoolCreateInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .pNext = nullptr,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = device.queueFamilyIndex_,
  };
  CALL_VK(vkCreateCommandPool(device.logic_, &cmdPoolCreateInfo, nullptr, &render.cmdPool));
}

void CreateCommandBuffers(void) {

  render.cmdBuffe.resize(swapchain.length);

  VkCommandBufferAllocateInfo cmdBufferCreateInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = nullptr,
      .commandPool = render.cmdPool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = static_cast<uint32_t>(render.cmdBuffe.size()),
  };
  CALL_VK(vkAllocateCommandBuffers(device.logic_, &cmdBufferCreateInfo, render.cmdBuffe.data()));

}

void CreateDepthStencil(void) {
  VkBool32 validDepthFormat = getSupportedDepthFormat(&depthStencil.format);
  assert(validDepthFormat);

  VkImageCreateInfo image = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = NULL,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = depthStencil.format,
      .extent = {
          swapchain.displaySize.width,
          swapchain.displaySize.height,
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

  CALL_VK(vkCreateImage(device.logic_, &image, nullptr, &depthStencil.image));
  vkGetImageMemoryRequirements(device.logic_, depthStencil.image, &memReqs);
  memAllocInfo.allocationSize = memReqs.size;
  assert(MapMemoryTypeToIndex(device.physical_, memReqs.memoryTypeBits,
                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memAllocInfo.memoryTypeIndex));

  CALL_VK(vkAllocateMemory(device.logic_, &memAllocInfo, nullptr, &depthStencil.mem));
  CALL_VK(vkBindImageMemory(device.logic_, depthStencil.image, depthStencil.mem, 0));

  depthStencilView.image = depthStencil.image;
  CALL_VK(vkCreateImageView(device.logic_, &depthStencilView, nullptr, &depthStencil.view));
}

void CreateRenderPass(void) {
  std::array<VkAttachmentDescription, 2> attachments = {};

  // Color attachment
  attachments[0].format = swapchain.displayFormat;
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

  CALL_VK(vkCreateRenderPass(device.logic_, &renderPassCreateInfo, nullptr, &render.renderPass));
}

void CreateFrameBuffers(void) {

  VkImageView attachments[2];
  // Depth/Stencil attachment is the same for all frame buffers
  attachments[1] = depthStencil.view;

  VkFramebufferCreateInfo fbCreateInfo{
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .pNext = nullptr,
      .renderPass = render.renderPass,
      .attachmentCount = 2,
      .pAttachments = attachments,
      .width = static_cast<uint32_t>(swapchain.displaySize.width),
      .height = static_cast<uint32_t>(swapchain.displaySize.height),
      .layers = 1,
  };

  // create a framebuffer from each swapchain image
  swapchain.framebuffers.resize(swapchain.length);

  for (uint32_t i = 0; i < swapchain.length; i++) {
    attachments[0] = swapchain.displayViews[i];
    CALL_VK(vkCreateFramebuffer(device.logic_, &fbCreateInfo, nullptr, &swapchain.framebuffers[i]));
  }
}

void CreateTexture(const char* filePath, struct Texture* texture) {
  LoadTextureFromFile(filePath, texture);

  const VkSamplerCreateInfo sampler = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .pNext = nullptr,
      .magFilter = VK_FILTER_NEAREST,
      .minFilter = VK_FILTER_NEAREST,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .mipLodBias = 0.0f,
      .maxAnisotropy = 1,
      .compareOp = VK_COMPARE_OP_NEVER,
      .minLod = 0.0f,
      .maxLod = (float)texture->mipLevels,
      .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
      .unnormalizedCoordinates = VK_FALSE,
  };
  CALL_VK(vkCreateSampler(device.logic_, &sampler, nullptr, &texture->sampler));

  VkImageViewCreateInfo view = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext = nullptr,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = texture->format,
      .components =
          {
              VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
              VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A,
          },
      .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, texture->mipLevels, 0, 1},
      .flags = 0,
      .image = texture->image,
  };

  CALL_VK(vkCreateImageView(device.logic_, &view, nullptr, &texture->view));
}

void CreateVertexDescriptions() {
  // Binding description
  vertices.bindingDescriptions.resize(1);
  vertices.bindingDescriptions[0].binding = 0;
  vertices.bindingDescriptions[0].stride = heartModel.layout.stride();
  vertices.bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  // Attribute descriptions
  // Describes memory layout and shader positions
  vertices.attributeDescriptions.resize(5); // TODO hardcoded
  // Location 0 : Position
  vertices.attributeDescriptions[0].binding = 0;
  vertices.attributeDescriptions[0].location = 0;
  vertices.attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
  vertices.attributeDescriptions[0].offset = 0;

  // Location 1 : Texture coordinates
  vertices.attributeDescriptions[1].binding = 0;
  vertices.attributeDescriptions[1].location = 1;
  vertices.attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
  vertices.attributeDescriptions[1].offset = sizeof(float) * 3;

  // Location 2 : Normal
  vertices.attributeDescriptions[2].binding = 0;
  vertices.attributeDescriptions[2].location = 2;
  vertices.attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
  vertices.attributeDescriptions[2].offset = sizeof(float) * 5;

  // Location 3 : Tangent
  vertices.attributeDescriptions[3].binding = 0;
  vertices.attributeDescriptions[3].location = 3;
  vertices.attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
  vertices.attributeDescriptions[3].offset = sizeof(float) * 8;

  // Location 3 : BiTangent
  vertices.attributeDescriptions[4].binding = 0;
  vertices.attributeDescriptions[4].location = 4;
  vertices.attributeDescriptions[4].format = VK_FORMAT_R32G32B32_SFLOAT;
  vertices.attributeDescriptions[4].offset = sizeof(float) * 11;


  VkPipelineVertexInputStateCreateInfo vertexInputInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .pNext = nullptr,
      .vertexBindingDescriptionCount = static_cast<uint32_t>(vertices.bindingDescriptions.size()),
      .pVertexBindingDescriptions = vertices.bindingDescriptions.data(),
      .vertexAttributeDescriptionCount = static_cast<uint32_t>(vertices.attributeDescriptions.size()),
      .pVertexAttributeDescriptions = vertices.attributeDescriptions.data(),
  };

  vertices.inputState = vertexInputInfo;
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
  CALL_VK(vkCreateBuffer(device.logic_, &createBufferInfo, nullptr, &uniformBuffer.buffer));

  VkMemoryRequirements memReq;
  vkGetBufferMemoryRequirements(device.logic_, uniformBuffer.buffer, &memReq);

  VkMemoryAllocateInfo memAllocInfo{
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = nullptr,
      .allocationSize = memReq.size,
      .memoryTypeIndex = 0,  // Memory type assigned in the next step
  };

  // Assign the proper memory type for that buffer
  memAllocInfo.allocationSize = memReq.size;
  assert(MapMemoryTypeToIndex(device.physical_, memReq.memoryTypeBits,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       &memAllocInfo.memoryTypeIndex));

  CALL_VK(vkAllocateMemory(device.logic_, &memAllocInfo, nullptr, &uniformBuffer.memory));

  CALL_VK(vkBindBufferMemory(device.logic_, uniformBuffer.buffer, uniformBuffer.memory, 0));

//  uniformBuffer.alignment = memReq.alignment;
//  uniformBuffer.size = memAllocInfo.allocationSize;
  uniformBuffer.descriptor.offset = 0;
  uniformBuffer.descriptor.range = sizeof(uboVS);
  uniformBuffer.descriptor.buffer = uniformBuffer.buffer;

  updateUniformBuffers();
 }

void CreateDescriptorSetLayout(void) {
  std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;

  VkDescriptorSetLayoutBinding vertexSetLayoutBinding {
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
      .pImmutableSamplers = nullptr
  };
  VkDescriptorSetLayoutBinding mainSamplerSetLayoutBinding {
      .binding = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .pImmutableSamplers = nullptr
  };
  VkDescriptorSetLayoutBinding normalSamplerSetLayoutBinding {
      .binding = 2,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .pImmutableSamplers = nullptr
  };

  setLayoutBindings.push_back(vertexSetLayoutBinding);
  setLayoutBindings.push_back(mainSamplerSetLayoutBinding);
  setLayoutBindings.push_back(normalSamplerSetLayoutBinding);

  VkDescriptorSetLayoutCreateInfo descriptorLayout{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = static_cast<uint32_t>(setLayoutBindings.size()),
      .pBindings = setLayoutBindings.data(),
  };

  CALL_VK(vkCreateDescriptorSetLayout(device.logic_, &descriptorLayout, nullptr, &descriptorSetLayout));
}

void CreatePipelineLayout(void) {

  VkPipelineCacheCreateInfo pipelineCacheInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
      .pNext = nullptr,
      .initialDataSize = 0,
      .pInitialData = nullptr,
      .flags = 0,  // reserved, must be 0
  };
  CALL_VK(vkCreatePipelineCache(device.logic_, &pipelineCacheInfo, nullptr, &pipelineCache));

  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .pNext = nullptr,
      .setLayoutCount = 1,
      .pSetLayouts = &descriptorSetLayout,
      .pushConstantRangeCount = 0,
      .pPushConstantRanges = nullptr,
  };
  CALL_VK(vkCreatePipelineLayout(device.logic_, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

}

VkResult LoadShaderFromFile(const char* filePath, VkShaderModule* shaderOut) {
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
  VkResult result = vkCreateShaderModule( device.logic_, &shaderModuleCreateInfo, nullptr, shaderOut);
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
      .cullMode = VK_CULL_MODE_BACK_BIT,
      .frontFace = VK_FRONT_FACE_CLOCKWISE,
      .depthBiasEnable = VK_FALSE,
      .lineWidth = 1.0f,
  };

  VkPipelineColorBlendAttachmentState blendAttachmentState{
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
      .blendEnable = VK_FALSE,
  };

  VkPipelineColorBlendStateCreateInfo colorBlendInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .pNext = nullptr,
      .attachmentCount = 1,
      .pAttachments = &blendAttachmentState,
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
  LoadShaderFromFile("shaders/heart.vert.spv", &vertexShader);
  LoadShaderFromFile("shaders/heart.frag.spv", &fragmentShader);

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


  // Create the pipeline
  VkGraphicsPipelineCreateInfo pipelineCreateInfo{
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .stageCount = 2,
      .pStages = shaderStages,
      .pVertexInputState = &vertices.inputState,
      .pInputAssemblyState = &inputAssemblyInfo,
      .pViewportState = &viewportInfo,
      .pRasterizationState = &rasterInfo,
      .pMultisampleState = &multisampleInfo,
      .pDepthStencilState = &depthStencilState,
      .pColorBlendState = &colorBlendInfo,
      .pDynamicState = &dynamicStateInfo,
      .layout = pipelineLayout,
      .renderPass = render.renderPass,
      .subpass = 0,
  };

  CALL_VK(vkCreateGraphicsPipelines(device.logic_, pipelineCache, 1,
                                    &pipelineCreateInfo, nullptr, &gfxPipeline));

  // We don't need the shaders anymore, we can release their memory
  vkDestroyShaderModule(device.logic_, vertexShader, nullptr);
  vkDestroyShaderModule(device.logic_, fragmentShader, nullptr);

}

void DeleteGraphicsPipeline(void) {
  if (gfxPipeline == VK_NULL_HANDLE) return;
  vkDestroyPipeline(device.logic_, gfxPipeline, nullptr);
  vkDestroyPipelineCache(device.logic_, pipelineCache, nullptr);
  vkDestroyPipelineLayout(device.logic_, pipelineLayout, nullptr);
}

void CreateSyncronization(void) {

  VkSemaphoreCreateInfo semaphoreCreateInfo{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
  };
  CALL_VK(vkCreateSemaphore(device.logic_, &semaphoreCreateInfo, nullptr, &render.semaphore));

  CALL_VK(vkCreateSemaphore(device.logic_, &semaphoreCreateInfo, nullptr, &swapchain.semaphore));
}

void CreateDescriptorPool(void) {
  std::vector<VkDescriptorPoolSize> poolSizes;

  VkDescriptorPoolSize descriptorPoolUniform{
      .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
  };
  VkDescriptorPoolSize descriptorPoolSample{
      .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 2,
  };

  poolSizes.push_back(descriptorPoolUniform);
  poolSizes.push_back(descriptorPoolSample);

  VkDescriptorPoolCreateInfo descriptorPoolInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
      .pPoolSizes = poolSizes.data(),
      .maxSets = 1,
  };

  CALL_VK(vkCreateDescriptorPool(device.logic_, &descriptorPoolInfo, nullptr, &descriptorPool));
}

void CreateDescriptorSet(void) {

  VkDescriptorSetAllocateInfo allocInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = descriptorPool,
      .pSetLayouts = &descriptorSetLayout,
      .descriptorSetCount = 1,
  };

  CALL_VK(vkAllocateDescriptorSets(device.logic_, &allocInfo, &descriptorSet));

  VkDescriptorImageInfo texMainDescriptor = {
      .sampler = heartMainTexture.sampler,
      .imageView = heartMainTexture.view,
      .imageLayout = heartMainTexture.layout,
  };
  VkDescriptorImageInfo texNormalDescriptor = {
      .sampler = heartNormalTexture.sampler,
      .imageView = heartNormalTexture.view,
      .imageLayout = heartNormalTexture.layout,
  };

  std::vector<VkWriteDescriptorSet> writeDescriptorSets;

  VkWriteDescriptorSet writeDescriptorSetUniform{
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = descriptorSet,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .dstBinding = 0,
      .pBufferInfo = &uniformBuffer.descriptor,
      .descriptorCount = 1,
  };
  VkWriteDescriptorSet writeDescriptorSetSampler{
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = descriptorSet,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .dstBinding = 1,
      .pImageInfo = &texMainDescriptor,
      .descriptorCount = 1,
  };
  VkWriteDescriptorSet writeDescriptorSetNormalSampler{
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = descriptorSet,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .dstBinding = 2,
      .pImageInfo = &texNormalDescriptor,
      .descriptorCount = 1,
  };


  writeDescriptorSets.push_back(writeDescriptorSetUniform);
  writeDescriptorSets.push_back(writeDescriptorSetSampler);
  writeDescriptorSets.push_back(writeDescriptorSetNormalSampler);

  vkUpdateDescriptorSets(device.logic_, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);
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
      .renderPass = render.renderPass,
      .renderArea.offset.x = 0,
      .renderArea.offset.y = 0,
      .renderArea.extent = swapchain.displaySize,
      .clearValueCount = 2,
      .pClearValues = clearValues};

  for (int32_t i = 0; i < render.cmdBuffe.size();  i++) {

    renderPassBeginInfo.framebuffer = swapchain.framebuffers[i];

    // We start by creating and declare the "beginning" our command buffer
    CALL_VK(vkBeginCommandBuffer(render.cmdBuffe[i], &cmdBufferBeginInfo));


    // Now we start a renderpass. Any draw command has to be recorded in a
    // renderpass

    vkCmdBeginRenderPass(render.cmdBuffe[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewports{
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
        .x = 0,
        .y = 0,
        .width = (float)swapchain.displaySize.width,
        .height = (float)swapchain.displaySize.height,
    };
    vkCmdSetViewport(render.cmdBuffe[i], 0, 1, &viewports);

    VkRect2D scissor = {
        .extent = swapchain.displaySize,
        .offset.x = 0,
        .offset.y = 0,
    };
    vkCmdSetScissor(render.cmdBuffe[i], 0, 1, &scissor);

    vkCmdBindDescriptorSets(render.cmdBuffe[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
                            0, 1, &descriptorSet, 0, nullptr);

    vkCmdBindPipeline(render.cmdBuffe[i], VK_PIPELINE_BIND_POINT_GRAPHICS, gfxPipeline);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(render.cmdBuffe[i], 0, 1,  &heartModel.vertices.buffer, &offset);

    // Bind triangle index buffer
    vkCmdBindIndexBuffer(render.cmdBuffe[i], heartModel.indices.buffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(render.cmdBuffe[i], heartModel.indexCount, 1, 0, 0, 0);

    vkCmdEndRenderPass(render.cmdBuffe[i]);

    CALL_VK(vkEndCommandBuffer(render.cmdBuffe[i]));
  }

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

  modelLoader = new ModelLoader(device.physical_, device.logic_, androidAppCtx);

  CreateSwapChain();
  CreateCommandPool();
  CreateCommandBuffers();
  CreateDepthStencil();
  CreateRenderPass();
  CreateFrameBuffers();

  // Setup model and load it in
  heartModel.layout = std::vector<ModelLoader::Component>(
      {ModelLoader::VERTEX_COMPONENT_POSITION,
       ModelLoader::VERTEX_COMPONENT_UV,
       ModelLoader::VERTEX_COMPONENT_NORMAL,
       ModelLoader::VERTEX_COMPONENT_TANGENT,
       ModelLoader::VERTEX_COMPONENT_BITANGENT,
      });
  heartModel.createInfo = ModelLoader::Model::CreateInfo(1.0f, 1.0f, 0.0f);
  modelLoader->LoadFromFile("models/heart/Heart.dae", &heartModel);

  CreateTexture("models/heart/heart_astc_8x8_main.ktx", &heartMainTexture);
  CreateTexture("models/heart/heart_astc_8x8_normal.ktx", &heartNormalTexture);

  CreateVertexDescriptions();
  CreateUniformBuffer();
  CreateDescriptorSetLayout();
  CreatePipelineLayout();
  CreateGraphicsPipeline();
  CreateSyncronization();
  CreateDescriptorPool();
  CreateDescriptorSet();



  BuildCommandBuffers();

  device.initialized_ = true;
  return true;
}

// IsVulkanReady():
//    native app poll to see if we are ready to draw...
bool IsVulkanReady(void) { return device.initialized_; }

void DeleteVulkan(void) {
  vkFreeCommandBuffers(device.logic_, render.cmdPool,
                       render.cmdBuffe.size(), render.cmdBuffe.data());

  vkDestroyCommandPool(device.logic_, render.cmdPool, nullptr);
  vkDestroyRenderPass(device.logic_, render.renderPass, nullptr);
  DeleteSwapChain();
  DeleteGraphicsPipeline();

  delete modelLoader;
  heartModel.destroy(device.logic_);

  vkDestroyDevice(device.logic_, nullptr);
  vkDestroyInstance(device.instance_, nullptr);

  device.initialized_ = false;
}

bool VulkanDrawFrame(void) {

  updateUniformBuffers();

  uint32_t nextIndex;
  // Get the framebuffer index we should draw in
  CALL_VK(vkAcquireNextImageKHR(
      device.logic_, swapchain.cmdBuffer, UINT64_MAX, render.semaphore, VK_NULL_HANDLE, &nextIndex));

  VkPipelineStageFlags waitStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submit_info = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                              .pNext = nullptr,
                              .waitSemaphoreCount = 1,
                              .pWaitSemaphores = &render.semaphore,
                              .pWaitDstStageMask = &waitStageMask,
                              .commandBufferCount = 1,
                              .pCommandBuffers = &render.cmdBuffe[nextIndex],
                              .signalSemaphoreCount = 1,
                              .pSignalSemaphores = &swapchain.semaphore};

  CALL_VK(vkQueueSubmit(device.queue_, 1, &submit_info, VK_NULL_HANDLE));

  CALL_VK(vkQueueWaitIdle(device.queue_));

  VkResult result;
  VkPresentInfoKHR presentInfo{
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .pNext = nullptr,
      .swapchainCount = 1,
      .pSwapchains = &swapchain.cmdBuffer,
      .pImageIndices = &nextIndex,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &swapchain.semaphore,
      .pResults = &result,
  };
  vkQueuePresentKHR(device.queue_, &presentInfo);
  return true;
}

/*
 * Android main functions to kick off native app
 */

// Process the next main command.
void handle_cmd(android_app* app, int32_t cmd) {
  switch (cmd) {
    case APP_CMD_INIT_WINDOW:
      // The window is being shown, get it ready.
      InitVulkan(app);
      break;
    case APP_CMD_TERM_WINDOW:
      // The window is being hidden or closed, clean it up.
      DeleteVulkan();
      break;
    default:
      LOGI("event not handled: %d", cmd);
  }
}

void android_main(struct android_app* app) {

  // Set the callback to process system events
  app->onAppCmd = handle_cmd;

  // Used to poll the events in the main loop
  int events;
  android_poll_source* source;

  // Main loop
  do {
    if (ALooper_pollAll(IsVulkanReady() ? 1 : 0, nullptr,
                        &events, (void**)&source) >= 0) {
      if (source != NULL) source->process(app, source);
    }

    // render if vulkan is ready
    if (IsVulkanReady()) {
      VulkanDrawFrame();
    }
  } while (app->destroyRequested == 0);
}
