/*
 * Vulkan Main
 * This class is ment to hold all the vulkan instance state
 * There is no need to encapsulate as a class as Android devices
 * only have the ability to run 1 Vulkan instances at a time
 */
#ifndef VULKAN_MAIN_HPP__
#define VULKAN_MAIN_HPP__

#include <android_native_app_glue.h>

#include <cassert>
#include <cstring>
#include <vector>
#include <array>

#include "vulkan_wrapper.h"
//#include "Cube.h"
#include "Debugging.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

// Comment out to remove Validation Layers
//#define VALIDATION_LAYERS

// Initialize vulkan device context
// after return, vulkan is ready to draw
bool InitVulkan(android_app* app);

// delete vulkan device context when application goes away
void DeleteVulkan(void);

// Check if vulkan is ready to draw
bool IsVulkanReady(void);

// Ask Vulkan to Render a frame
bool VulkanDrawFrame(void);

#endif // VULKAN_MAIN_HPP__


