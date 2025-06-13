#include "voxel.h"
#include "../buffer.h"
#include <vulkan/vulkan_core.h>

#include <iostream>

class Grid {
    private:

    Voxel* voxels;

    public:

    Buffer buf;

    void create() {

        voxels = new Voxel[15 * 15 * 15];

        for(int x = 0; x < 15; x++) {
            for(int y = 0; y < 15; y++) {
                for (int z = 0; z < 15; z++) {

                    if((x- 7) * (x - 7) + (y - 7)*(y - 7) + (z - 7)*(z - 7) <= 16) {
                        voxels[x * 15 * 15 + y * 15 + z] = {1};
                    }
                    else {
                        voxels[x * 15 * 15 + y * 15 + z] = {0};
                    }
                
                }
            }
        }
    }

    void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool transferPool, VkQueue transferQueue) {
        buf.createBuffer(device, physicalDevice, 15 * 15 * 15 * 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, true);
        buf.populateBuffer(device, physicalDevice, voxels, 15 * 15 * 15 * 4, transferPool, transferQueue);

        delete [] voxels;
    }

    void destroy(VkDevice device) {
        buf.destroy(device);
    }
};