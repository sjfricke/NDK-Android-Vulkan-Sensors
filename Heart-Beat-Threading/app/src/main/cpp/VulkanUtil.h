#ifndef __VULKAN_UTIL_HPP__
#define __VULKAN_UTIL_HPP__

#include "vulkan_wrapper.h"

#include <android/log.h>

// Comment out to remove Validation Layers
#define VALIDATION_LAYERS

namespace navs {

static const char *APPLICATION_NAME = "Heart_Beat_Threading";

// Android log function wrappers
static const char *kTAG = "HeartBeat";
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


static bool MapMemoryTypeToIndex(VkPhysicalDevice pDevice, uint32_t typeBits,
                                 VkFlags requirements_mask, uint32_t* typeIndex) {
  VkPhysicalDeviceMemoryProperties memoryProperties;
  vkGetPhysicalDeviceMemoryProperties(pDevice, &memoryProperties);
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

} // navs namespace
#endif // __VULKAN_UTIL_HPP__