#ifndef __CUBE_HPP__
#define __CUBE_HPP__

#include <android_native_app_glue.h>

// Initialize vulkan device context
// after return, vulkan is ready to draw
bool InitVulkan(android_app* app);

// delete vulkan device context when application goes away
void DeleteVulkan(void);

// Check if vulkan is ready to draw
bool IsVulkanReady(void);

// Ask Vulkan to Render a frame
bool VulkanDrawFrame(void);

#endif // __CUBE_HPP__


