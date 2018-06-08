#include "ModelLoader.h"

#include <android/asset_manager.h>

#include "VulkanUtil.h"

#define TINYGLTF_IMPLEMENTATION
#include "tiny_gltf.h"

using namespace navs;

ModelLoader::ModelLoader(VkPhysicalDevice pDevice, VkDevice lDevice, android_app* app) :
    mLogicDevice(lDevice),
    mPhysicalDevice(pDevice),
    androidAppCtx(app)
{
}

ModelLoader::~ModelLoader() {

}
#include <iostream>
void ModelLoader::LoadFromFile(const char* filePath, Model* model)
{
  tinygltf::Model gltfModel;
  tinygltf::TinyGLTF gltfContext;
  std::string baseDir;
  std::string error;
  std::vector<Vertex> vertexBuffer;
  std::vector<uint32_t> indexBuffer;

  assert(androidAppCtx);
  AAsset* file = AAssetManager_open(androidAppCtx->activity->assetManager, filePath, AASSET_MODE_BUFFER);

  size_t fileLength = AAsset_getLength(file);
  assert(fileLength > 0);
  char* fileData = new char[fileLength];

  AAsset_read(file, (void*)fileData, fileLength);
  AAsset_close(file);


  bool fileLoaded = gltfContext.LoadASCIIFromString(&gltfModel, &error, fileData, fileLength, baseDir);
  assert(fileLoaded);
  const tinygltf::Scene &scene = gltfModel.scenes[gltfModel.defaultScene];

  for (size_t i = 0; i < scene.nodes.size(); i++) {
    const tinygltf::Node node = gltfModel.nodes[scene.nodes[i]];
    loadNode(node, glm::mat4(1.0f), gltfModel, indexBuffer, vertexBuffer, scale);
  }

  delete[] fileData;


  uint32_t vertexBufferSize = static_cast<uint32_t>(vertexBuffer.size()) * sizeof(Vertex);
  uint32_t indexBufferSize = static_cast<uint32_t>(indexBuffer.size()) * sizeof(uint32_t);
  model->indexCount = static_cast<uint32_t>(indexBuffer.size());
  assert((vertexBufferSize > 0) && (indexBufferSize > 0));

  // Vertex buffer
  CALL_VK(CreateBuffer(
      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      vertexBufferSize,
      &model->vertices.buffer,
      &model->vertices.memory,
      vertexBuffer.data()));

  // Index buffer
  CALL_VK(CreateBuffer(
      VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      indexBufferSize,
      &model->indices.buffer,
      &model->indices.memory,
      indexBuffer.data()));
}

VkResult ModelLoader::CreateBuffer(VkBufferUsageFlags usageFlags,
                                   VkMemoryPropertyFlags memoryPropertyFlags,
                                   VkDeviceSize size, VkBuffer *buffer, VkDeviceMemory *memory,
                                   void *data) {

  // Create a vertex buffer
  VkBufferCreateInfo bufferCreateInfo{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = nullptr,
      .size = size,
      .usage = usageFlags,
      .flags = 0,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .pQueueFamilyIndices = 0, // HARDCODED since Android should have only 1 queue, should fix
      .queueFamilyIndexCount = 1,
  };

  CALL_VK(vkCreateBuffer(mLogicDevice, &bufferCreateInfo, nullptr, buffer));

  VkMemoryRequirements memReq;
  vkGetBufferMemoryRequirements(mLogicDevice, *buffer, &memReq);

  VkMemoryAllocateInfo memAllocInfo{
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = nullptr,
      .allocationSize = memReq.size,
      .memoryTypeIndex = 0,  // Memory type assigned in the next step
  };

  // Assign the proper memory type for that buffer
  assert(MapMemoryTypeToIndex(mPhysicalDevice, memReq.memoryTypeBits,
                              memoryPropertyFlags, &memAllocInfo.memoryTypeIndex));

  // Allocate memory for the buffer
  CALL_VK(vkAllocateMemory(mLogicDevice, &memAllocInfo, nullptr, memory));

  // If a pointer to the buffer data has been passed, map the buffer and copy over the data
  if (data != nullptr)
  {
    void *mapped;
    CALL_VK(vkMapMemory(mLogicDevice, *memory, 0, size, 0, &mapped));
    memcpy(mapped, data, size);

    // If host coherency hasn't been requested, do a manual flush to make writes visible
    if ((memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0) {
      VkMappedMemoryRange mappedMemoryRange {
          .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
          .memory = *memory,
          .offset = 0,
          .size = size
      };
      vkFlushMappedMemoryRanges(mLogicDevice, 1, &mappedMemoryRange);
    }

    vkUnmapMemory(mLogicDevice, *memory);
  }

  // Attach the memory to the buffer object
  CALL_VK(vkBindBufferMemory(mLogicDevice, *buffer, *memory, 0));

  return VK_SUCCESS;
}