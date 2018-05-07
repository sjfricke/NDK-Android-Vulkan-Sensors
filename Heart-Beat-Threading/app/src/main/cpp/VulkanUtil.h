/*
 * This is basically the global variable file.
 * I justify this since this is an Android Vulkan app
 * which will only have 1 device and makes sharing the info
 * instead of creating a whole Vulkan engine set of files for
 * these more simple demos
 */
#ifndef __VULKAN_UTIL_HPP__
#define __VULKAN_UTIL_HPP__

#include "vulkan_wrapper.h"
#include "ValidationLayers.h"

#include <android/log.h>

// Comment out to remove Validation Layers
#define VALIDATION_LAYERS

const char* APPLICATION_NAME = "Heart_Beat_Threading";

// Android log function wrappers
static const char* kTAG = "HeartBeat";
#define LOGI(...) \
  ((void)__android_log_print(ANDROID_LOG_INFO, kTAG, __VA_ARGS__))
#define LOGW(...) \
  ((void)__android_log_print(ANDROID_LOG_WARN, kTAG, __VA_ARGS__))
#define LOGE(...) \
  ((void)__android_log_print(ANDROID_LOG_ERROR, kTAG, __VA_ARGS__))

// Vulkan call wrapper
#define CALL_VK(func)                                                 \
  if (VK_SUCCESS != (func)) {                                         \
    __android_log_print(ANDROID_LOG_ERROR, "HeartBeat ",              \
                        "Vulkan error. File[%s], line[%d]", __FILE__, \
                        __LINE__);                                    \
    assert(false);                                                    \
  }

// Global Variables ...
struct VulkanDeviceInfo {
  bool initialized_;

  VkInstance instance_;
  VkPhysicalDevice physical_;
  VkDevice logic_;
  uint32_t queueFamilyIndex_;

  VkSurfaceKHR surface_;
  VkQueue queue_;
};
VulkanDeviceInfo device;

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
  device.physical_ = tmpGpus[0];  // Pick up the first GPU Device

#ifdef VALIDATION_LAYERS
  layerAndExt.InitDevLayersAndExt(device.physical_);
#endif

  // Find a GFX queue family
  uint32_t queueFamilyCount;
  vkGetPhysicalDeviceQueueFamilyProperties(device.physical_, &queueFamilyCount, nullptr);
  assert(queueFamilyCount);
  std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(device.physical_, &queueFamilyCount, queueFamilyProperties.data());

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

  CALL_VK(vkCreateDevice(device.physical_, &deviceCreateInfo, nullptr, &device.logic_));
  vkGetDeviceQueue(device.logic_, device.queueFamilyIndex_, 0, &device.queue_);
}

#endif // __VULKAN_UTIL_HPP__