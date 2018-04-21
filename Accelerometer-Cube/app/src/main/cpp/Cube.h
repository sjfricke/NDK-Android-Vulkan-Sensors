#ifndef __CUBE_HPP__
#define __CUBE_HPP__

#include <android_native_app_glue.h>
#include "vulkan_wrapper.h"
#include "Debugging.h"
#include <vector>

struct Vertex {
  float position[3];
  float color[3];
};

// Vertex buffer and attributes
struct {
  VkDeviceMemory memory;
  VkBuffer buffer;
} vertices;

// Index buffer
struct
{
  VkDeviceMemory memory;
  VkBuffer buffer;
  uint32_t count;
} indices;

// Setup vertices
std::vector<Vertex> vertexBuffer =
    {
        { {  1.0f,  1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f } },
        { { -1.0f,  1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f } },
        { { -1.0f, -1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f } },
        { {  1.0f, -1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f } },

        { {  1.0f,  1.0f, -1.0f }, { 0.0f, 1.0f, 0.0f } },
        { { -1.0f,  1.0f, -1.0f }, { 0.0f, 1.0f, 0.0f } },
        { { -1.0f, -1.0f, -1.0f }, { 0.0f, 1.0f, 0.0f } },
        { {  1.0f, -1.0f, -1.0f }, { 0.0f, 1.0f, 0.0f } },

        { {  1.0f, -1.0f,  1.0f }, { 0.0f, 0.0f, 1.0f } },
        { { -1.0f, -1.0f,  1.0f }, { 0.0f, 0.0f, 1.0f } },
        { { -1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f, 1.0f } },
        { {  1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f, 1.0f } },

        { {  1.0f,  1.0f,  1.0f }, { 1.0f, 1.0f, 0.0f } },
        { { -1.0f,  1.0f,  1.0f }, { 1.0f, 1.0f, 0.0f } },
        { { -1.0f,  1.0f, -1.0f }, { 1.0f, 1.0f, 0.0f } },
        { {  1.0f,  1.0f, -1.0f }, { 1.0f, 1.0f, 0.0f } },

        { {  1.0f,  1.0f, -1.0f }, { 1.0f, 0.0f, 1.0f } },
        { {  1.0f,  1.0f,  1.0f }, { 1.0f, 0.0f, 1.0f } },
        { {  1.0f, -1.0f,  1.0f }, { 1.0f, 0.0f, 1.0f } },
        { {  1.0f, -1.0f, -1.0f }, { 1.0f, 0.0f, 1.0f } },

        { { -1.0f,  1.0f, -1.0f }, { 0.0f, 1.0f, 1.0f } },
        { { -1.0f,  1.0f,  1.0f }, { 0.0f, 1.0f, 1.0f } },
        { { -1.0f, -1.0f,  1.0f }, { 0.0f, 1.0f, 1.0f } },
        { { -1.0f, -1.0f, -1.0f }, { 0.0f, 1.0f, 1.0f } }
    };
uint32_t vertexBufferSize = static_cast<uint32_t>(vertexBuffer.size()) * sizeof(Vertex);

// Setup indices
std::vector<uint32_t> indexBuffer = {
    0, 1, 2, 0, 2, 3,
    4, 5, 6, 4, 6, 7,
    8, 9, 10, 8, 10, 11,
    12, 13, 14, 12, 14, 15,
    16, 17, 18, 16, 18, 19,
    20, 21, 22, 20, 22, 23
};
indices.count = static_cast<uint32_t>(indexBuffer.size());
uint32_t indexBufferSize = indices.count * sizeof(uint32_t);

#endif // __CUBE_HPP__


