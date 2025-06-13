#include "application.h"
#include <GLFW/glfw3.h>

#include <cstdint>
#include <map>
#include <stdexcept>
#include <utility>
#include <vulkan/vulkan_core.h>

void Application::run() {

    init_window();

	//All functions here will initialize vulkan
	create_instance();
	createSurface();
	setupDebugMessenger();
	pickPhysicalDevices();
	createLogicalDevice();

	createSwapchain();
	createImageViews();

	createCommandPools();

	raytracer.createRayTracer(device, physicalDevice, graphicsQueue, graphicsPool, transferQueue, transferPool, swapchainFormat, swapchainExtent, window);

    main_loop();

	raytracer.cleanup();

    cleanup();
}

void Application::init_window() {
    glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	window = glfwCreateWindow(width, height, "Vulkan Render", nullptr, nullptr);
	glfwSetWindowUserPointer(window, this);

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	glfwSetFramebufferSizeCallback(window, frameBufferResizeCallBack);
	glfwSetCursorPosCallback(window, mousePosCallBack);

	glfwSwapBuffers(window);
}

bool Application::checkValidationLayerSupport() {
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	vector<VkLayerProperties> layerProperties(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, layerProperties.data());

	for(const char* name : validationLayers) {

		bool layerFound = false;

		for(const auto& layerProperty : layerProperties) {
			if(strcmp(name, layerProperty.layerName) == 0) {
				layerFound = true;
				break;
			}
		}

		if(!layerFound) return false;
	}

	return true;
}

vector<const char*> Application::getRequiredExtensions() {
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	if (enableValidationLayers) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	return extensions;
}

void Application::create_instance() {
	uint32_t extensionPropertiesCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionPropertiesCount, nullptr);
	vector<VkExtensionProperties> extensionsProperties(extensionPropertiesCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionPropertiesCount, extensionsProperties.data());
	

	VkApplicationInfo app_info{};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "Vulkan Renderer";
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName = "No Engine";
	app_info.apiVersion = VK_API_VERSION_1_2;

	//get the extensions
	uint32_t glfwExtensionCount = 0;
	const char** glfw_extensions;
	glfw_extensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	VkInstanceCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &app_info;

	//get the required extensions 
	auto extensions = getRequiredExtensions();
	create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	create_info.ppEnabledExtensionNames = extensions.data();

	//check for validation layers and put them in the strust if enabled
	if(enableValidationLayers && !checkValidationLayerSupport()) throw runtime_error("Validation layers requested but not available");

	if(enableValidationLayers) {
		create_info.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		create_info.ppEnabledLayerNames = validationLayers.data();
	}
	else {
		create_info.enabledLayerCount = 0;
	}

	if(vkCreateInstance(&create_info, nullptr, &instance) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create instance");
	}
}

void Application::setupDebugMessenger() {
	if(!enableValidationLayers) return;

	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = debugCallback;
	createInfo.pUserData = nullptr;

	//since this is an extension function we need to load it ourselves as extension functions are not loaded by default
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

	if(func == nullptr || func(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) throw runtime_error("Failed to setuo debug messenger");
}

QueueFamily Application::findQueueFamilies(VkPhysicalDevice dev) {
	QueueFamily queueFamily;

	uint32_t queueFamilyCount;
	vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, nullptr);

	vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, queueFamilyProperties.data());

	int i = 0;

	for(const auto& properties : queueFamilyProperties) {

		if(properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			queueFamily.graphicsFamily = i;
		}

		VkBool32 presetationQueuePresent = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &presetationQueuePresent);

		if(presetationQueuePresent) {
			queueFamily.presentationFamily = i;
		}

		if(properties.queueFlags & VK_QUEUE_TRANSFER_BIT && !(properties.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
			queueFamily.transferFamily = i;
		}

		if(queueFamily.isComplete()) break;

		i++;

	}

	return queueFamily;
}

SwapChainSupportDetails Application::querySwapChainSupport(VkPhysicalDevice dev) {
	SwapChainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, surface, &details.capabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface,&formatCount, nullptr);

	if(formatCount != 0) {
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &formatCount, details.formats.data());
	}

	uint32_t presentCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &presentCount, nullptr);

	if(presentCount != 0) {
		details.presetMode.resize(presentCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &presentCount, details.presetMode.data());
	}

	return details;
}

bool Application::checkDeviceExtensionsSupport(VkPhysicalDevice dev) {
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(dev, nullptr, &extensionCount, nullptr);

	vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(dev, nullptr,&extensionCount, extensions.data());

	set<string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

	for(const auto& extension: extensions) {
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}

int Application::rateSuitability(VkPhysicalDevice dev) {
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(dev, &deviceProperties);

	QueueFamily qf = findQueueFamilies(dev);

	if(!qf.isComplete()) return 0;
	if(!checkDeviceExtensionsSupport(dev)) return 0;
	

	SwapChainSupportDetails details = querySwapChainSupport(dev);

	if(details.presetMode.empty() || details.formats.empty()) return 0;

	int score = 0;

	if(deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 100;

	return score;
}
 
void Application::pickPhysicalDevices() {
	uint32_t deviceCount;
	vkEnumeratePhysicalDevices(instance, &deviceCount,nullptr);

	vector<VkPhysicalDevice> physicalDevices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices.data());

	multimap<int, VkPhysicalDevice> scoreTable;

	for(const auto& devices : physicalDevices) {
		int score = rateSuitability(devices);
		scoreTable.insert(make_pair(score, devices));
	}

	if(scoreTable.rbegin()->first > 0) {
		physicalDevice = scoreTable.rbegin()->second;
	}
	else {
		throw runtime_error("Failed to find a physical device");
	}
}

void Application::createLogicalDevice() {
	QueueFamily indices = findQueueFamilies(physicalDevice);
	vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentationFamily.value(), indices.transferFamily.value() };
	float queuePriority = 1.0f;

	for (uint32_t queueFamily : uniqueQueueFamilies) {
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);	
	}

	// Buffer Device Address feature
	VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{};
	bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
	bufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;

	// Acceleration Structure feature
	VkPhysicalDeviceAccelerationStructureFeaturesKHR accelStructFeatures{};
	accelStructFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
	accelStructFeatures.accelerationStructure = VK_TRUE;
	accelStructFeatures.pNext = &bufferDeviceAddressFeatures;

	// Ray Tracing Pipeline feature
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures{};
	rayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
	rayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
	rayTracingPipelineFeatures.pNext = &accelStructFeatures;

	// Root device features structure
	VkPhysicalDeviceFeatures2 deviceFeatures2{};
	deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	deviceFeatures2.pNext = &rayTracingPipelineFeatures;

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.pEnabledFeatures = VK_NULL_HANDLE;

	createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();
	createInfo.pNext = &deviceFeatures2;

	if (enableValidationLayers) {
    	createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
    	createInfo.ppEnabledLayerNames = validationLayers.data();
	} 
	else {
    	createInfo.enabledLayerCount = 0;
	}

	if(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
		throw runtime_error("Failed to create logical device");
	}

	vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
	vkGetDeviceQueue( device, indices.presentationFamily.value(),0, &presentationQueue);
	vkGetDeviceQueue(device, indices.transferFamily.value(), 0,&transferQueue);
}

void Application::createSurface() {
	if(glfwCreateWindowSurface(instance,window, nullptr, &surface) != VK_SUCCESS) {
		throw runtime_error("Failed to create surface");
	}
}

VkSurfaceFormatKHR Application::chooseSurfaceFormat(const vector<VkSurfaceFormatKHR>& formats) {
	for(const auto& format : formats) {
		if(format.format == VK_FORMAT_B8G8R8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return format;
	}

	return formats[0];
}

VkPresentModeKHR Application::choosePresentMode(const vector<VkPresentModeKHR>& presentModes) {
	for(const auto& presentationMode: presentModes) {
		if(presentationMode == VK_PRESENT_MODE_MAILBOX_KHR) return presentationMode;
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Application::chooseSwapChainExtent(VkSurfaceCapabilitiesKHR capabilities) {

	if(capabilities.currentExtent.width != numeric_limits<uint32_t>::max()) return capabilities.currentExtent;

	int width, height;
	glfwGetFramebufferSize(window, &width, &height);

	VkExtent2D extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

	extent.width = clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
	extent.height = clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

	return extent;
}

void Application::createSwapchain() {
	SwapChainSupportDetails supportDetails = querySwapChainSupport(physicalDevice);

	VkSurfaceFormatKHR format = chooseSurfaceFormat(supportDetails.formats);
	VkPresentModeKHR presentMode = choosePresentMode(supportDetails.presetMode);
	VkExtent2D extent = chooseSwapChainExtent(supportDetails.capabilities);

	uint32_t imageCount = supportDetails.capabilities.minImageCount;

	if(imageCount < supportDetails.capabilities.maxImageCount && supportDetails.capabilities.maxImageCount > 0) imageCount = supportDetails.capabilities.maxImageCount;

	VkSwapchainCreateInfoKHR createInfo{};

	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.imageFormat = format.format;
	createInfo.imageColorSpace = format.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	createInfo.surface = surface;
	createInfo.minImageCount = imageCount;

	QueueFamily family = findQueueFamilies(physicalDevice);
	uint32_t queues[] = {family.graphicsFamily.value(), family.presentationFamily.value()};

	if(queues[0] != queues[1]) {
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    	createInfo.queueFamilyIndexCount = 2;
    	createInfo.pQueueFamilyIndices = queues;
	}
	else {
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    	createInfo.queueFamilyIndexCount = 0;
    	createInfo.pQueueFamilyIndices = nullptr; 
	}

	createInfo.preTransform = supportDetails.capabilities.currentTransform;

	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

	createInfo.oldSwapchain = VK_NULL_HANDLE;

	if(vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain) != VK_SUCCESS) {
		throw runtime_error("Failed to create swapchain");
	}

	vkGetSwapchainImagesKHR(device, swapchain,&imageCount,nullptr);
	swapchainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());

	swapchainFormat = format;
	swapchainPresentMode = presentMode;
	swapchainExtent = extent;
}

void Application::recreateSwapchain() {
	vkDeviceWaitIdle(device);
	cleanupSwapchain();

    createSwapchain();
    createImageViews();
}

void Application::createImageViews() {
	swapchainImageViews.resize(swapchainImages.size());

	for(size_t i = 0; i < swapchainImages.size(); i++) {
		VkImageViewCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = swapchainImages[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = swapchainFormat.format;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		if(vkCreateImageView(device, &createInfo, nullptr, &swapchainImageViews[i]) != VK_SUCCESS) {
			throw runtime_error("Failed to create image view");
		}

	}
}

void Application::createCommandPools() {
	QueueFamily familes = findQueueFamilies(physicalDevice);

	VkCommandPoolCreateInfo graphicPoolInfo{};
	graphicPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	graphicPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	graphicPoolInfo.queueFamilyIndex = familes.graphicsFamily.value();

	if(vkCreateCommandPool(device, &graphicPoolInfo, nullptr, &graphicsPool) != VK_SUCCESS) {
		throw runtime_error("Failed to create graphics pool");
	}

	VkCommandPoolCreateInfo transferPoolInfo{};
	transferPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	transferPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	transferPoolInfo.queueFamilyIndex = familes.transferFamily.value();

	if(vkCreateCommandPool(device, &transferPoolInfo, nullptr, &transferPool) != VK_SUCCESS) {
		throw runtime_error("Failed to create transfer pool");
	}
}

////////////////////////////////////////// IMPROVE SRNCRONIZATION THIS SHIT IS ASSS IMPROVE THIS PLEASEE REMBER TO IMPROVE THIS HAHAHAHAHHAHAHAHAHAHH H HH FU FENUFNFE JFE FUCKKKKKKKKKKKKKKKKKKKKKKKKKKKK ///////////////////////////////////////////////////
void Application::main_loop() {
	VkFenceCreateInfo fenceCreateInfo{};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkFence inFlightFence;
	VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &inFlightFence), "failed to create fence");

	VkSemaphoreCreateInfo semCreateInfo{};
	semCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkSemaphore imageSemaphore, renderSemaphore;
	VK_CHECK(vkCreateSemaphore(device, &semCreateInfo, nullptr, &imageSemaphore), "Failed to create semaphore");
	VK_CHECK(vkCreateSemaphore(device, &semCreateInfo, nullptr, &renderSemaphore), "Failed to create semaphore");

	CommandBuffer commandBuffer;
	commandBuffer.createCommandBuffer(device, graphicsPool);

	float lastFrame = 0;

	VkResult resize;

	while(!glfwWindowShouldClose(window)){
		float deltaTime = glfwGetTime() - lastFrame;
		lastFrame = glfwGetTime();
		
		vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
		vkResetFences(device, 1, &inFlightFence);

		vkResetCommandBuffer( commandBuffer.handle, 0);

		uint32_t imageIndex;
    	resize = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageSemaphore, VK_NULL_HANDLE, &imageIndex);

		if(resize == VK_ERROR_OUT_OF_DATE_KHR || resize == VK_SUBOPTIMAL_KHR) {
			recreateSwapchain();
		}

		raytracer.drawFrame(commandBuffer, swapchainImages[imageIndex], deltaTime);

		VkPipelineStageFlags stageFlags[] = {VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR};

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &renderSemaphore;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &imageSemaphore;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer.handle;
		submitInfo.pWaitDstStageMask = stageFlags;

		VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence), "Failed to submit to queue");

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swapchain;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nullptr;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &renderSemaphore;

		resize = vkQueuePresentKHR(presentationQueue, &presentInfo);

		resize = vkQueueWaitIdle(presentationQueue);

		if(resize == VK_ERROR_OUT_OF_DATE_KHR) recreateSwapchain();

		cout << "FPS : " << (1 / deltaTime) << endl;


		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	vkDeviceWaitIdle(device);

	commandBuffer.freeCommandBuffer(device, graphicsPool);

	vkDestroyFence(device, inFlightFence, nullptr);
	vkDestroySemaphore(device, imageSemaphore, nullptr);
	vkDestroySemaphore(device, renderSemaphore, nullptr);
}

void Application::cleanupSwapchain() {
	for(auto imageView: swapchainImageViews) {
		vkDestroyImageView(device, imageView, nullptr);
	}

	vkDestroySwapchainKHR(device, swapchain, nullptr);
}

void Application::cleanup() {

	vkDestroyCommandPool(device, graphicsPool, nullptr);
	vkDestroyCommandPool(device, transferPool, nullptr);

	cleanupSwapchain();

	vkDestroyDevice(device, nullptr);
	vkDestroySurfaceKHR(instance,surface, nullptr);

	if(enableValidationLayers) {
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if(func != nullptr) {
			func(instance, debugMessenger, nullptr);
		}
	}

	vkDestroyInstance(instance, nullptr);

	glfwDestroyWindow(window);
	glfwTerminate();
}