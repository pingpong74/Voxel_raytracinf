#pragma once

#include "../buffer.h"
#include "../commandBuffer.h"
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>
#include <stdexcept>
#include <vector>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "../DataStructures/grid.h"
#include "accelerationStructure.h"
#include "../Camera.h"

using namespace std;

#define VK_CHECK(name, err) \
if(name != VK_SUCCESS) { throw runtime_error(err); }

struct CameraConstants {
    glm::mat4 inverseView;
    glm::mat4 inverseProj;
};

struct ShaderBindingTable {

    Buffer buffer;

    VkStridedDeviceAddressRegionKHR rgenRegion{};
    VkStridedDeviceAddressRegionKHR missRegion{};
    VkStridedDeviceAddressRegionKHR hitRegion{};
    VkStridedDeviceAddressRegionKHR callRegion{};
};

class RayTracer {
    public:

    Camera cam;

    void createRayTracer(VkDevice _device, VkPhysicalDevice _physicalDevice, VkQueue _graphicsQueue, VkCommandPool _graphicsPool, VkQueue _transferQueue, VkCommandPool _transferPool, VkSurfaceFormatKHR format, VkExtent2D extent, GLFWwindow* window);
    void drawFrame(CommandBuffer commandBuffer, VkImage swapchainImage, float deltaTime);
    void cleanup();

    //Handling resize
    void handleResize(VkSurfaceFormatKHR format, VkExtent2D extent);

    private:

    VkDevice device;
    VkPhysicalDevice physicalDevice;

    VkCommandPool graphicsPool;
    VkQueue graphicsQueue;

    VkCommandPool transferPool;
    VkQueue transferQueue;

    GLFWwindow* window;

    //Pipeline creation function
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR = nullptr;

    //Get Properties
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;

    //trace rays
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;

    //functions for pipeline creation
    VkShaderModule createShaderModule(const string& filePath);
    VkPipelineShaderStageCreateInfo createShaderStageCreateInfo(VkShaderModule shaderModule, VkShaderStageFlagBits flags);

    //Descriptor sets
    VkDescriptorPool descriptorPool;
    VkDescriptorSet set0;
    VkDescriptorSetLayout set0Layout;

    //Raytracing pipeline
    vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups;
    VkPipelineLayout rayTracingPipelineLayout;
    VkPipeline rayTracingPipeline;

    //Pipeline and acceleration strucutre properties
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties{};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{};

    //Testing
    vector<AccelerationStructure> blases;
    AccelerationStructure tlas;
    Buffer testBuffer;

    //Images shit
    VkImage frame;
    VkDeviceMemory imgMemory;
    VkImageView frameView;
    VkExtent2D imgExtent;

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void setImgLayout(CommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageSubresourceRange subResourcesRange, VkPipelineStageFlags srcFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VkPipelineStageFlags dstFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    //Shader Binding table
    Buffer sbtBuffer;

    VkStridedDeviceAddressRegionKHR rgenRegion{};
    VkStridedDeviceAddressRegionKHR missRegion{};
    VkStridedDeviceAddressRegionKHR hitRegion{};
    VkStridedDeviceAddressRegionKHR callRegion{};
    
    //Loads all the functions for extensions 
    void loadFunctions();

    AccelerationStructure buildBLAS();
    AccelerationStructure buildTLAS(std::vector<AccelerationStructure> blases);

    //The storage image which the pipeline will write too
    void createImage(VkSurfaceFormatKHR format, VkExtent2D extent);

    //Creaete the camera buffers for the descriptor
    Buffer ubo;
    void createUBOBuffer();

    //Creation of descriptor sets
    void createDescritorSets();
    void updateDescriptorSets(float deltaTime);

    //Pipeline and binidng table. Binding table is used for fast look up of shaders
    void createRayTracingPipeline();
    void createShaderBindingTable();

    //Rendering code
    void recordCommandBuffer(CommandBuffer commandBuffer, VkImage swapchainImage);

    //FINISHHHHHH 
    void destroyAccelerationStructure(AccelerationStructure acccelerationStructure);
    
};