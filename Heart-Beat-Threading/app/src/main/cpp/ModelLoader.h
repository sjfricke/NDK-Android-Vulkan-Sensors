#ifndef __MODEL_LOADER_HPP__
#define __MODEL_LOADER_HPP__

#include <android_native_app_glue.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>

#include "vulkan_wrapper.h"

class ModelLoader {
 private:
  VkDevice mLogicDevice = nullptr;
  VkPhysicalDevice mPhysicalDevice = nullptr;
  android_app* androidAppCtx = nullptr;
  VkResult CreateBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags,
      VkDeviceSize size, VkBuffer *buffer, VkDeviceMemory *memory, void *data = nullptr);

  struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
  };

 public:

  typedef enum Component {
    VERTEX_COMPONENT_POSITION = 0x0,
    VERTEX_COMPONENT_NORMAL = 0x1,
    VERTEX_COMPONENT_COLOR = 0x2,
    VERTEX_COMPONENT_UV = 0x3,
    VERTEX_COMPONENT_TANGENT = 0x4,
    VERTEX_COMPONENT_BITANGENT = 0x5,
    VERTEX_COMPONENT_DUMMY_FLOAT = 0x6,
    VERTEX_COMPONENT_DUMMY_VEC4 = 0x7
  } Component;

  struct Model {
    struct {
      VkBuffer buffer;
      VkDeviceMemory memory;
    } vertices;

    struct {
      VkBuffer buffer;
      VkDeviceMemory memory;
    } indices;

    uint32_t indexCount = 0;
    uint32_t vertexCount = 0;

    struct ModelPart {
      uint32_t vertexBase;
      uint32_t vertexCount;
      uint32_t indexBase;
      uint32_t indexCount;
    };
    std::vector<ModelPart> parts;

    struct VertexLayout {
     public:
      /** @brief Components used to generate vertices from */
      std::vector<Component> components;
      VertexLayout() {};

      VertexLayout(std::vector<Component> components)
      {
        this->components = std::move(components);
      }

      uint32_t stride()
      {
        uint32_t res = 0;
        for (auto& component : components)
        {
          switch (component)
          {
            case VERTEX_COMPONENT_UV:
              res += 2 * sizeof(float);
              break;
            case VERTEX_COMPONENT_DUMMY_FLOAT:
              res += sizeof(float);
              break;
            case VERTEX_COMPONENT_DUMMY_VEC4:
              res += 4 * sizeof(float);
              break;
            default:
              // All components except the ones listed above are made up of 3 floats
              res += 3 * sizeof(float);
          }
        }
        return res;
      }
    } layout;

    struct CreateInfo {
      glm::vec3 center;
      glm::vec3 scale;
      glm::vec2 uvscale;

      CreateInfo() {};

      CreateInfo(glm::vec3 scale, glm::vec2 uvscale, glm::vec3 center)
      {
        this->center = center;
        this->scale = scale;
        this->uvscale = uvscale;
      }

      CreateInfo(float scale, float uvscale, float center)
      {
        this->center = glm::vec3(center);
        this->scale = glm::vec3(scale);
        this->uvscale = glm::vec2(uvscale);
      }

    } createInfo;

    // Destroys all Vulkan resources created for this model
    void destroy(VkDevice device)
    {
      vkDestroyBuffer(device, vertices.buffer, nullptr);
      vkFreeMemory(device, vertices.memory, nullptr);
      vkDestroyBuffer(device, indices.buffer, nullptr);
      vkFreeMemory(device, indices.memory, nullptr);
    };
  };


  ModelLoader(VkPhysicalDevice pDevice, VkDevice lDevice, android_app* app);
  ~ModelLoader();
  void LoadFromFile(const char* filePath, Model* model);
};


#endif // __MODEL_LOADER_HPP__


