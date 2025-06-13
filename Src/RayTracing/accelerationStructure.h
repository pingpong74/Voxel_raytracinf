
#include <vulkan/vulkan_core.h>
#include <vector>
#include "../buffer.h"

#define LOAD_FUNC(device, name) \
name = (PFN_##name)vkGetDeviceProcAddr(device, #name); \
if(!name) { throw std::runtime_error("Failed to load function"); }

struct PackedInstanceKHR {
    float transform[3][4];
    uint32_t customIndex_mask; // 4 bytes: 24 bits custom index + 8 bits mask
    uint32_t sbtRecordOffset_flags;    // 4 bytes: 24 bits offset + 8 bits flags
    uint64_t blasDeviceAddress;  
};


struct AccelerationStructure {
    VkAccelerationStructureKHR handle;
    Buffer buffer;


    static inline PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
    static inline PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
    static inline PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
    static inline PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = nullptr;
    static inline PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR = nullptr;

    static inline VkDevice device = nullptr;
    static inline VkPhysicalDevice physicalDevice = nullptr;

    static void loadFunctions(VkDevice device, VkPhysicalDevice physicalDevice) {
        AccelerationStructure::device = device;
        AccelerationStructure::physicalDevice = physicalDevice;

        LOAD_FUNC(device, vkGetAccelerationStructureDeviceAddressKHR);
        LOAD_FUNC(device, vkGetAccelerationStructureBuildSizesKHR);
        LOAD_FUNC(device, vkCreateAccelerationStructureKHR);
        LOAD_FUNC(device, vkCmdBuildAccelerationStructuresKHR);
        LOAD_FUNC(device, vkDestroyAccelerationStructureKHR);
    }

    static VkDeviceAddress getBLASAddress(AccelerationStructure* acccelerationStructure) {
        VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
        addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        addressInfo.accelerationStructure = acccelerationStructure->handle;

        return vkGetAccelerationStructureDeviceAddressKHR(device, &addressInfo);
    }

    //For future reference: once multiple blases will be be built, all data will be accuired from one huge buffer and hence the offesets and buildrangeinfo will be used. not yet done
    static AccelerationStructure createBottomLevelAccelereationStructure(Buffer boundingBoxBuffer, VkCommandPool buildPool, VkQueue buildQueue) {

        //Fill in the geometry struct which contains the data we wanna turn in to BLAS
        VkAccelerationStructureGeometryAabbsDataKHR aabbData{};
        aabbData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
        aabbData.data.deviceAddress = boundingBoxBuffer.getBufferAddress(device);
        aabbData.stride = sizeof(VkAabbPositionsKHR);
        aabbData.pNext = nullptr;

        //More info on this shit
        VkAccelerationStructureGeometryKHR geometry{};
        geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geometry.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
        geometry.geometry.aabbs = aabbData;
        geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        geometry.pNext = nullptr;

        //Gives info on how the data is laid out
        VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
        buildRangeInfo.primitiveCount = 1;
        buildRangeInfo.primitiveOffset = 0;
        buildRangeInfo.transformOffset = 0;
        buildRangeInfo.firstVertex = 0;

        //Information on how this data needs to be built
        VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo{};
        buildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildGeometryInfo.pNext = nullptr;
        buildGeometryInfo.srcAccelerationStructure = VK_NULL_HANDLE;
        buildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        buildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        buildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildGeometryInfo.geometryCount = 1;
        buildGeometryInfo.pGeometries = &geometry;
        buildGeometryInfo.ppGeometries = nullptr;

        //Gets the information on the size of scratch buffer and the size of accceleration structure in memory
        VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
        sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        uint32_t count = 1;
        vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildGeometryInfo, &count, &sizeInfo);

        //Make a scratch buffer from the info gathered
        Buffer scratchBuffer;
        scratchBuffer.createBuffer(device, physicalDevice, sizeInfo.buildScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false);

        //assign the scratch buffer
        buildGeometryInfo.scratchData.deviceAddress = scratchBuffer.getBufferAddress(device);
        
        //make the buffer in which the acceleration strcuture will reside
        Buffer accelerationStructureBuffer;
        accelerationStructureBuffer.createBuffer(device, physicalDevice, sizeInfo.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false);

        //create the acceleration struture 
        VkAccelerationStructureCreateInfoKHR accelerationCreateInfo;
        accelerationCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        accelerationCreateInfo.buffer = accelerationStructureBuffer.handle;
        accelerationCreateInfo.size = sizeInfo.accelerationStructureSize;
        accelerationCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        accelerationCreateInfo.pNext = VK_NULL_HANDLE;
        accelerationCreateInfo.offset = 0;
        accelerationCreateInfo.createFlags = 0;
        accelerationCreateInfo.deviceAddress = 0;

        VkAccelerationStructureKHR accelerationStructure;
        vkCreateAccelerationStructureKHR(device, &accelerationCreateInfo, nullptr, &accelerationStructure);

        buildGeometryInfo.dstAccelerationStructure = accelerationStructure;

        //FINALLY send the acceleration strucuture off the the gpu where it is built and will stay for the rest of its life before it is destoryed 
        VkAccelerationStructureBuildRangeInfoKHR* infos[] = { &buildRangeInfo };

        CommandBuffer commandBuffer;
        commandBuffer.createCommandBuffer(device, buildPool);
        commandBuffer.beginRecording(true);

        vkCmdBuildAccelerationStructuresKHR(commandBuffer.handle, 1, &buildGeometryInfo, (VkAccelerationStructureBuildRangeInfoKHR* const*)&infos);

        commandBuffer.endRecording();

        //submit to the queue
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer.handle;

        vkQueueSubmit(buildQueue, 1, &submitInfo, VK_NULL_HANDLE);

        vkQueueWaitIdle(buildQueue);

        //Destroy all the useless shit
        scratchBuffer.destroy(device);
        boundingBoxBuffer.destroy(device);

        return {accelerationStructure, accelerationStructureBuffer};

    }

    static AccelerationStructure createTopLevelAccelerationStructure(std::vector<AccelerationStructure> blases, std::vector<VkTransformMatrixKHR> transforms, VkCommandPool buildPool, VkQueue buildQueue, VkCommandPool transferPool, VkQueue transferQueue) {
        std::vector<VkAccelerationStructureInstanceKHR> instances;

        for(int i = 0; i < blases.size(); i++) {
            /*PackedInstanceKHR instance;
            memcpy(&instance.transform, &transforms[i], 48);
            instance.blasDeviceAddress = getBLASAddress(&blases[i]);
            instance.customIndex_mask = (1 << 24) | (0xFF);
            instance.sbtRecordOffset_flags = VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR << 24; // leaving sbt offest as 0*/

            VkAccelerationStructureInstanceKHR instance{};
		    instance.transform = transforms[i];
		    instance.instanceCustomIndex = 0;
		    instance.mask = 0xFF;
		    instance.instanceShaderBindingTableRecordOffset = 0;
		    instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		    instance.accelerationStructureReference = getBLASAddress(&blases[i]);

            instances.push_back(instance);
        }

        //create a buffer on the gpu which has the data in the exact format wanted by vulkan
        Buffer instanceBuffer;
        instanceBuffer.createBuffer(device, physicalDevice, sizeof(VkAccelerationStructureInstanceKHR) * instances.size(), VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, true);
        instanceBuffer.populateBuffer(device, physicalDevice, instances.data(), sizeof(VkAccelerationStructureInstanceKHR) * instances.size(), transferPool, transferQueue);

        //Fill the data related to the instances
        VkAccelerationStructureGeometryInstancesDataKHR instanceData{};
        instanceData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        instanceData.data.deviceAddress = instanceBuffer.getBufferAddress(device);

        VkAccelerationStructureGeometryKHR geometry{};
        geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        geometry.geometry.instances = instanceData;

        VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
        buildRangeInfo.primitiveCount = blases.size();
        buildRangeInfo.firstVertex = 0;
        buildRangeInfo.primitiveOffset = 0;
        buildRangeInfo.transformOffset = 0;

        VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo{};
        buildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildGeometryInfo.pNext = nullptr;
        buildGeometryInfo.srcAccelerationStructure = VK_NULL_HANDLE;
        buildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        buildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        buildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildGeometryInfo.geometryCount = 1;
        buildGeometryInfo.pGeometries = &geometry;
        buildGeometryInfo.ppGeometries = nullptr;

        //Get info about the size of the acceleration structure and also the scratch buffer required
        VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
        sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        uint32_t count = 1;
        vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildGeometryInfo, &count, &sizeInfo);

        //Make a scratch buffer from the info gathered
        Buffer scratchBuffer;
        scratchBuffer.createBuffer(device, physicalDevice, sizeInfo.buildScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false);

        //assign the scratch buffer
        buildGeometryInfo.scratchData.deviceAddress = scratchBuffer.getBufferAddress(device);

        //make the buffer in which the acceleration strcuture will reside
        Buffer accelerationStructureBuffer;
        accelerationStructureBuffer.createBuffer(device, physicalDevice, sizeInfo.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false);

        //create acceleeration structure
        VkAccelerationStructureCreateInfoKHR accelerationCreateInfo;
        accelerationCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        accelerationCreateInfo.buffer = accelerationStructureBuffer.handle;
        accelerationCreateInfo.size = sizeInfo.accelerationStructureSize;
        accelerationCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        accelerationCreateInfo.pNext = VK_NULL_HANDLE;
        accelerationCreateInfo.offset = 0;
        accelerationCreateInfo.createFlags = 0;
        accelerationCreateInfo.deviceAddress = 0;

        VkAccelerationStructureKHR accelerationStructure;
        vkCreateAccelerationStructureKHR(device, &accelerationCreateInfo, nullptr, &accelerationStructure);

        buildGeometryInfo.dstAccelerationStructure = accelerationStructure;

        //FINALLY send the acceleration strucuture off the the gpu where it is built and will stay for the rest of its life before it is destoryed 
        VkAccelerationStructureBuildRangeInfoKHR* infos[] = { &buildRangeInfo };

        CommandBuffer commandBuffer;
        commandBuffer.createCommandBuffer(device, buildPool);
        commandBuffer.beginRecording(true);

        vkCmdBuildAccelerationStructuresKHR(commandBuffer.handle, 1, &buildGeometryInfo, (VkAccelerationStructureBuildRangeInfoKHR* const*)&infos);

        commandBuffer.endRecording();

        //submit to the queue
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer.handle;

        vkQueueSubmit(buildQueue, 1, &submitInfo, VK_NULL_HANDLE);

        vkQueueWaitIdle(buildQueue);


        scratchBuffer.destroy(device);
        instanceBuffer.destroy(device);

        return {accelerationStructure, accelerationStructureBuffer};
    }

    static void destroyAccelerationStructure(AccelerationStructure accelerationStructure) {
        accelerationStructure.buffer.destroy(device);
        vkDestroyAccelerationStructureKHR(device, accelerationStructure.handle, nullptr);
    }
};