#ifndef __BUFFERS_HPP__
#define __BUFFERS_HPP__

#include "VulkanUtil.h"

namespace navs {

bool MapMemoryTypeToIndex(uint32_t typeBits, VkFlags requirements_mask, uint32_t* typeIndex) {
  VkPhysicalDeviceMemoryProperties memoryProperties;
  vkGetPhysicalDeviceMemoryProperties(device.physical_, &memoryProperties);
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

// Creates generic buffer
VkResult CreateBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags,
                      VkDeviceSize size, VkBuffer *buffer, VkDeviceMemory *memory, void *data) {

  // Create a vertex buffer
  VkBufferCreateInfo bufferCreateInfo{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = nullptr,
      .size = size,
      .usage = usageFlags,
      .flags = 0,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .pQueueFamilyIndices = &device.queueFamilyIndex_,
      .queueFamilyIndexCount = 1,
  };

  CALL_VK(vkCreateBuffer(device.logic_, &bufferCreateInfo, nullptr, buffer));

  VkMemoryRequirements memReq;
  vkGetBufferMemoryRequirements(device.logic_, *buffer, &memReq);

  VkMemoryAllocateInfo memAllocInfo{
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = nullptr,
      .allocationSize = memReq.size,
      .memoryTypeIndex = 0,  // Memory type assigned in the next step
  };

  // Assign the proper memory type for that buffer
  assert(MapMemoryTypeToIndex(memReq.memoryTypeBits, memoryPropertyFlags, &memAllocInfo.memoryTypeIndex));

  // Allocate memory for the buffer
  CALL_VK(vkAllocateMemory(device.logic_, &memAllocInfo, nullptr, memory));

  // If a pointer to the buffer data has been passed, map the buffer and copy over the data
  if (data != nullptr)
  {
    void *mapped;
    CALL_VK(vkMapMemory(device.logic_, *memory, 0, size, 0, &mapped));
    memcpy(mapped, data, size);

    // If host coherency hasn't been requested, do a manual flush to make writes visible
    if ((memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0) {
      VkMappedMemoryRange mappedMemoryRange {
          .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
          .memory = *memory,
          .offset = 0,
          .size = size
      };
      vkFlushMappedMemoryRanges(device.logic_, 1, &mappedMemoryRange);
    }

    vkUnmapMemory(device.logic_, *memory);
  }

  // Attach the memory to the buffer object
  CALL_VK(vkBindBufferMemory(device.logic_, *buffer, *memory, 0));

  return VK_SUCCESS;
}

} // navs namespace


#endif // __BUFFERS_HPP__


