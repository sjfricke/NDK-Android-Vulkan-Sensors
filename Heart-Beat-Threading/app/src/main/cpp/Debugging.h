/*
 * Debbuging is used to hold logging info in seperate file
 * Also a place to put future Validation Layers for debugging
 */
#ifndef __DEBUGGING_HPP__
#define __DEBUGGING_HPP__

#include <android/log.h>

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


#endif // __DEBUGGING_HPP__


