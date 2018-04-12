#ifndef __CUBE_HPP__
#define __CUBE_HPP__

#include <android_native_app_glue.h>
#include "linmath.h"

// Initialize vulkan device context
// after return, vulkan is ready to draw
bool InitVulkan(android_app* app);

// delete vulkan device context when application goes away
void DeleteVulkan(void);

// Check if vulkan is ready to draw
bool IsVulkanReady(void);

// Ask Vulkan to Render a frame
bool VulkanDrawFrame(void);

// Vertex positions
const float vertexData[] = {
    // Face 1 (Front)
    1.0f,  1.0f,  1.0f, // 0
    -1.0f,  1.0f,  1.0f, // 1
    -1.0f, -1.0f,  1.0f, // 2
    -1.0f, -1.0f,  1.0f, // 3
    1.0f, -1.0f,  1.0f, // 2
    1.0f,  1.0f,  1.0f, // 0
    // Face 2 (Back)
    1.0f, -1.0f, -1.0f, // 4
    -1.0f,  1.0f, -1.0f, // 5
    1.0f,  1.0f, -1.0f, // 6
    -1.0f,  1.0f, -1.0f, // 5
    1.0f, -1.0f, -1.0f, // 4
    -1.0f, -1.0f, -1.0f, // 7
    // Face 3 (Top)
    1.0f,  1.0f,  1.0f, // 0
    1.0f,  1.0f, -1.0f, // 6
    -1.0f,  1.0f, -1.0f, // 5
    -1.0f,  1.0f, -1.0f, // 5
    -1.0f,  1.0f,  1.0f, // 1
    1.0f,  1.0f,  1.0f, // 0
    // Face 4 (Bottom)
    1.0f, -1.0f,  1.0f, // 2
    -1.0f, -1.0f,  1.0f, // 3
    -1.0f, -1.0f, -1.0f, // 7
    -1.0f, -1.0f, -1.0f, // 7
    1.0f, -1.0f, -1.0f, // 4
    1.0f, -1.0f,  1.0f, // 2
    // Face 5 (Left)
    -1.0f, -1.0f,  1.0f, // 3
    -1.0f,  1.0f,  1.0f, // 1
    -1.0f,  1.0f, -1.0f, // 5
    -1.0f, -1.0f,  1.0f, // 4
    -1.0f,  1.0f, -1.0f, // 5
    -1.0f, -1.0f, -1.0f, // 3
    // Face 6 (Right)
    1.0f,  1.0f,  1.0f, // 0
    1.0f, -1.0f,  1.0f, // 2
    1.0f, -1.0f, -1.0f, // 4
    1.0f, -1.0f, -1.0f, // 4
    1.0f,  1.0f, -1.0f, // 6
    1.0f,  1.0f,  1.0f, // 0
};


const float normalData[] = {
    // Face 1 (Front)
    0.0f, 0.0f, 1.0f,
    0.0f, 0.0f, 1.0f,
    0.0f, 0.0f, 1.0f,
    0.0f, 0.0f, 1.0f,
    0.0f, 0.0f, 1.0f,
    0.0f, 0.0f, 1.0f,
    // Face 2 (Back)
    0.0f, 0.0f, -1.0f,
    0.0f, 0.0f, -1.0f,
    0.0f, 0.0f, -1.0f,
    0.0f, 0.0f, -1.0f,
    0.0f, 0.0f, -1.0f,
    0.0f, 0.0f, -1.0f,
    // Face 3 (Top)
    0.0f, 1.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    // Face 4 (Bottom)
    0.0f, -1.0f, 0.0f,
    0.0f, -1.0f, 0.0f,
    0.0f, -1.0f, 0.0f,
    0.0f, -1.0f, 0.0f,
    0.0f, -1.0f, 0.0f,
    0.0f, -1.0f, 0.0f,
    // Face 5 (Left)
    -1.0f, 0.0f, 0.0f,
    -1.0f, 0.0f, 0.0f,
    -1.0f, 0.0f, 0.0f,
    -1.0f, 0.0f, 0.0f,
    -1.0f, 0.0f, 0.0f,
    -1.0f, 0.0f, 0.0f,
    // Face 6 (Right)
    1.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f,
};

const float colorData[] = {
    // Face 1 (Front) BLUE
    0.0f, 0.0f, 1.0f,
    0.0f, 0.0f, 1.0f,
    0.0f, 0.0f, 1.0f,
    0.0f, 0.0f, 1.0f,
    0.0f, 0.0f, 1.0f,
    0.0f, 0.0f, 1.0f,
    // Face 2 (Back) BLUE
    0.0f, 0.0f, 1.0f,
    0.0f, 0.0f, 1.0f,
    0.0f, 0.0f, 1.0f,
    0.0f, 0.0f, 1.0f,
    0.0f, 0.0f, 1.0f,
    0.0f, 0.0f, 1.0f,
    // Face 3 (Top) GREEN
    0.0f, 1.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    // Face 4 (Bottom)GREEN
    0.0f, 1.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    // Face 5 (Left) RED
    1.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f,
    // Face 6 (Right) RED
    1.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f,
};

#endif // __CUBE_HPP__


