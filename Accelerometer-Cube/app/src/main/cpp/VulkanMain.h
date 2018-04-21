/*
 * Vulkan Main
 * This class is ment to hold all the vulkan instance state
 * There is no need to encapsulate as a class as Android devices
 * only have the ability to run 1 Vulkan instances at a time
 */
#ifndef __VULKAN_HPP__
#define __VULKAN_HPP__

#include <android_native_app_glue.h>

#include <cassert>
#include <cstring>
#include <vector>
#include <array>

#include "vulkan_wrapper.h"
#include "Cube.h"
#include "Debugging.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

const char* APPLICATION_NAME = "Accelerometer_Cube";

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
  uint32_t cmdBufferLen_;
  VkSemaphore semaphore_;
  VkFence fence_;
};
VulkanRenderInfo render;

struct VulkanBufferInfo {
  VkBuffer buffer;
  VkDeviceMemory memory;
  VkDescriptorBufferInfo descriptor;
  VkDeviceSize size;
  VkDeviceSize alignment;
  VkBufferUsageFlags usageFlags;
  VkMemoryPropertyFlags memoryPropertyFlags;
  void* mapped = nullptr;
};
VulkanBufferInfo uniformBuffer;
VulkanBufferInfo vertexBuffer;

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
  glm::mat4 projection;
  glm::mat4 modelView;
  glm::vec4 lightPos = glm::vec4(0.0f, 2.0f, 1.0f, 0.0f);
} uboVS;

float zoom = -10.5f;
glm::vec3 rotation = glm::vec3(-25.0f, 15.0f, 0.0f);
glm::vec3 cameraPos = glm::vec3();

// Initialize vulkan device context
// after return, vulkan is ready to draw
bool InitVulkan(android_app* app);

// delete vulkan device context when application goes away
void DeleteVulkan(void);

// Check if vulkan is ready to draw
bool IsVulkanReady(void);

// Ask Vulkan to Render a frame
bool VulkanDrawFrame(void);

#endif // __VULKAN_HPP__


