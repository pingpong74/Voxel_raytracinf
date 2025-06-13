#pragma once 

#include <stdexcept>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

class CommandBuffer {

    public:

    VkCommandBuffer handle;

    void createCommandBuffer(VkDevice device ,VkCommandPool commandPool) {
        VkCommandBufferAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocateInfo.commandBufferCount = 1;
        allocateInfo.commandPool = commandPool;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.pNext = VK_NULL_HANDLE;

        if(vkAllocateCommandBuffers(device, &allocateInfo, &handle) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffer");
        }
    }

    void beginRecording(bool useOnce) {

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        if(useOnce) beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        if(vkBeginCommandBuffer(handle, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to start recording command buffer");
        }
    }

    void endRecording() {
        if(vkEndCommandBuffer(handle) != VK_SUCCESS) {
            throw std::runtime_error("Failed to stop recording command buffer");
        }
    }

    void freeCommandBuffer(VkDevice device, VkCommandPool commandPool) {
        vkFreeCommandBuffers(device, commandPool, 1, &handle);
    }
};