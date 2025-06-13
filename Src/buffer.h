#pragma once

#include "commandBuffer.h"
#include <cstring>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

class Buffer {
    private:

    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        for(uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if( (typeFilter & (1 << i)) && ( (memProperties.memoryTypes[i].propertyFlags & properties) == properties) ) {
                return i;
            }
        }

        throw std::runtime_error("Failed to find suitable memory type for the buffer");
    }


    public:

    VkBuffer handle;
    VkDeviceMemory bufferMemory;

    //ADD SHARING MODES IN FUTURE PLEASE
    void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, bool isStaged) {
        VkBufferCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        createInfo.size = (VkDeviceSize)size;

        if(isStaged) createInfo.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        else createInfo.usage = usage;

        createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if(vkCreateBuffer(device, &createInfo, nullptr, &handle) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create buffer");
        }

        VkMemoryRequirements memoryRequirements;
        vkGetBufferMemoryRequirements(device, handle, &memoryRequirements);


        VkMemoryAllocateFlagsInfo flagInfo{};
        flagInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        flagInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

        VkMemoryAllocateInfo memoryAllocInfo{};
        memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memoryAllocInfo.allocationSize = memoryRequirements.size;
        memoryAllocInfo.pNext = &flagInfo;
        memoryAllocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memoryRequirements.memoryTypeBits, properties);

        if(vkAllocateMemory(device, &memoryAllocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate memory for buffer");
        }

        if(vkBindBufferMemory(device, handle, bufferMemory, 0) != VK_SUCCESS ) {
            throw std::runtime_error("Failed to bind buffer memory");
        }
    }

    void populateBuffer(VkDevice device, VkPhysicalDevice physicalDevice, const void* data, VkDeviceSize size, VkCommandPool transferPool, VkQueue transferQueue) {

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;

        VkBufferCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        createInfo.size = size;
        createInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if(vkCreateBuffer(device, &createInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Faioled to create staging buffer");
        }

        VkMemoryRequirements memoryRequirements;
        vkGetBufferMemoryRequirements(device, stagingBuffer, &memoryRequirements);

        VkMemoryAllocateInfo memoryAllocInfo{};
        memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memoryAllocInfo.allocationSize = memoryRequirements.size;
        memoryAllocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memoryRequirements.memoryTypeBits,  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if(vkAllocateMemory(device, &memoryAllocInfo, nullptr, &stagingBufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate memory");
        }

        if(vkBindBufferMemory(device, stagingBuffer, stagingBufferMemory, 0) != VK_SUCCESS ) {
            throw std::runtime_error("Failed to bind buffer memory");
        }


        void* dst;

        vkMapMemory(device, stagingBufferMemory, 0, size, 0, &dst);
        memcpy(dst, data, (size_t)size);
        vkUnmapMemory(device, stagingBufferMemory);

        CommandBuffer commandBuffer;
        commandBuffer.createCommandBuffer(device, transferPool);
        commandBuffer.beginRecording(true);

        VkBufferCopy copyRegion;
        copyRegion.dstOffset = 0;
        copyRegion.srcOffset = 0;
        copyRegion.size = size;

        vkCmdCopyBuffer(commandBuffer.handle, stagingBuffer, handle, 1, &copyRegion);

        commandBuffer.endRecording();

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer.handle;

        vkQueueSubmit(transferQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(transferQueue);

        commandBuffer.freeCommandBuffer(device, transferPool);

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }

    VkDeviceAddress getBufferAddress(VkDevice device) {
        VkBufferDeviceAddressInfo addressInfo{};
        addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        addressInfo.buffer = handle;

        return vkGetBufferDeviceAddress(device, &addressInfo);
    }

    void destroy(VkDevice device) {
        vkDestroyBuffer(device, handle, nullptr);
        vkFreeMemory(device, bufferMemory, nullptr);
    }
};