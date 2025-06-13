#pragma once
#include "RayTracing/raytracer.h"
#include <vulkan/vulkan_core.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <map>
#include <optional>
#include <set>
#include <limits>
#include <algorithm>
#include <fstream>

//REMEMBER TO HAVE A COMPUTE QUEUE AND OFF LOAD BUILDING ACCELERATION STRUCTURE TO THAT QUEUE

using namespace std;

const uint32_t width = 800;
const uint32_t height = 600;

const vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation"};
const vector<const char*> deviceExtensions = { "VK_KHR_swapchain",
    "VK_KHR_acceleration_structure",
    "VK_KHR_ray_tracing_pipeline",
    "VK_KHR_ray_query",
    "VK_KHR_pipeline_library",
    "VK_KHR_deferred_host_operations",
 };

const bool enableValidationLayers = true;

struct QueueFamily {
    optional<uint32_t> graphicsFamily;
    optional<uint32_t> presentationFamily;
    optional<uint32_t> transferFamily;
    //optional<uint32_t> computeFamily;

    bool isComplete() {
        return graphicsFamily.has_value() && presentationFamily.has_value() && transferFamily.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    vector<VkSurfaceFormatKHR> formats;
    vector<VkPresentModeKHR> presetMode;
};

class Application {
    public:

    void run();

    void mouseInput(double xpos, double ypos) {
        raytracer.cam.MouseInput(window, xpos, ypos);
    } 

    void rayTracerResize() {
        recreateSwapchain();
        raytracer.handleResize(swapchainFormat, swapchainExtent);
    }

    private:

    GLFWwindow* window;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkDevice device;

    VkQueue graphicsQueue;
    VkQueue presentationQueue;
    VkQueue transferQueue;

    VkPhysicalDevice physicalDevice;
    VkSurfaceKHR surface;
    VkSurfaceFormatKHR swapchainFormat;
    VkPresentModeKHR swapchainPresentMode;
    VkExtent2D swapchainExtent;
    VkSwapchainKHR swapchain;

    VkCommandPool graphicsPool;
    VkCommandPool transferPool;

    RayTracer raytracer;
    
    vector<VkImage> swapchainImages;
    vector<VkImageView> swapchainImageViews;

    

    static void frameBufferResizeCallBack(GLFWwindow* window, int width, int height) {
        Application* app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
        app->rayTracerResize();
    }

    static void mousePosCallBack(GLFWwindow *window, double xpos, double ypos) {
        Application* app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
        app->mouseInput( xpos, ypos);
    }

    //Sends the error msg to console
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
		if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
			cerr << "validationlayer:" << pCallbackData->pMessage << endl;
		}

		return VK_FALSE;
	}

    //functions for validations layers
    bool checkValidationLayerSupport();
    vector<const char*> getRequiredExtensions();

    void init_window();

    void create_instance();
    void setupDebugMessenger();

    QueueFamily findQueueFamilies(VkPhysicalDevice device);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice dev);
    bool checkDeviceExtensionsSupport(VkPhysicalDevice dev);
    int rateSuitability(VkPhysicalDevice dev);
    void pickPhysicalDevices();

    void createLogicalDevice();

    void createSurface();

    //swapchain creation 
    VkSurfaceFormatKHR chooseSurfaceFormat(const vector<VkSurfaceFormatKHR>& formats);
    VkPresentModeKHR choosePresentMode(const vector<VkPresentModeKHR>& presentMode);
    VkExtent2D chooseSwapChainExtent(VkSurfaceCapabilitiesKHR capabilities);
    void createSwapchain();
    void recreateSwapchain();

    void createImageViews();
    void createFramebuffers();

    void createCommandPools();

    //Creating the shaders shit
    VkShaderModule createShaderModule(const string& filePath);
    VkPipelineShaderStageCreateInfo createShaderStageCreateInfo(VkShaderModule shaderModule, VkShaderStageFlagBits flags);

    void main_loop();

    void cleanupSwapchain();
    void cleanup();
};