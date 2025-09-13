#include "VulkanEngine.h"

#include <stdexcept>
#include <cstdlib>
#include <memory>
#include <set>
#include <algorithm>
#include <string>
#include <iostream>
#include <chrono>
#include <thread>

#include <vulkan/vulkan_hpp_macros.hpp>

#include "ECS.h"
#include "InputManager.h"
#include "DebugWindow.h"
#include "AssetManager.h"
#include "Components.h"
#include "EntitySearcher.h"
#include "LightSystem.h"
#include "Renderer3D.h"
#include "TransformSystem.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
	const vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
	vk::DebugUtilsMessageTypeFlagsEXT,
	const vk::DebugUtilsMessengerCallbackDataEXT* pData,
	void*
) {
	const char* sev;
	switch (severity) {
		case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
			sev = "INFO";
			break;
		case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
			sev = "VERBOSE";
			break;
		case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
			sev = "WARN";
			break;
		case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
			sev = "ERROR";
			break;
		default:
			sev = "UNKNOWN";
	}
	std::cerr << "[VULKAN_" << sev << "] " << pData->pMessage << std::endl;
	return vk::False;
}

void VulkanEngine::run() {
	initWindow();
	initVulkan();
	initECS();
	createScene();
	mainLoop();
	cleanup();
}

DebugWindow* VulkanEngine::getDebugWindow() const {
	return m_debugWindow.get();
}

FrameTimeInfo VulkanEngine::getFrameTimeInfo() const {
	auto [result, data] = m_queryPool.getResults<u64>(0, 2, 2 * sizeof(u64), sizeof(u64), vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait);
	const float nsPerTick = m_physicalDevice.getProperties().limits.timestampPeriod;
	return {
		.frameTime = m_deltaTime * 1000, // glfwGetTime returns seconds, so *1000 to convert to millis
		.gpuTime = static_cast<float>(data[1] - data[0]) * nsPerTick / 1000000.0f, // gpu time is measured in nanoseconds, so divide by 1,000,000 to get millis
		.cpuTime = m_cpuTime
	};
}

VRAMUsageInfo VulkanEngine::getVramUsage() const {
	auto result = m_physicalDevice.getMemoryProperties2<vk::PhysicalDeviceMemoryProperties2, vk::PhysicalDeviceMemoryBudgetPropertiesEXT>();
	auto properties = result.get<vk::PhysicalDeviceMemoryProperties2>().memoryProperties;
	auto budget = result.get<vk::PhysicalDeviceMemoryBudgetPropertiesEXT>();
	VRAMUsageInfo info;
	for (u32 i = 0; i < properties.memoryHeapCount; ++i) {
		if (properties.memoryHeaps[i].flags & vk::MemoryHeapFlagBits::eDeviceLocal) {
			info.gpuTotal += properties.memoryHeaps[i].size;
			info.gpuUsed += budget.heapUsage[i];
			info.gpuAvailable += budget.heapBudget[i];
		}
		else if (!properties.memoryHeaps[i].flags){
			info.sharedTotal += properties.memoryHeaps[i].size;
			info.sharedUsed += budget.heapUsage[i];
			info.sharedAvailable += budget.heapBudget[i];
		}
		else {
			Logger::warn("Unknown heap memory type");
		}
	}
	return info;
}

const vk::raii::Device & VulkanEngine::getDevice() const {
	return m_device;
}

const vk::raii::PhysicalDevice & VulkanEngine::getPhysicalDevice() const {
	return m_physicalDevice;
}

const vk::raii::DescriptorPool & VulkanEngine::getDescriptorPool() const {
	return m_descriptorPool;
}

Renderer3D* VulkanEngine::getRenderer() const {
	return m_renderer;
}

void VulkanEngine::setPresentMode(vk::PresentModeKHR mode) {
	m_presentMode = mode;
	queueSwapRecreation();
}

vk::PresentModeKHR VulkanEngine::getPresentMode() const {
	return m_presentMode;
}

vk::Format VulkanEngine::getSwapColourFormat() {
	return vk::Format::eB8G8R8A8Unorm;
}

vk::Format VulkanEngine::getDepthFormat() {
	return vk::Format::eD32Sfloat;
}

void VulkanEngine::queueSwapRecreation() {
	m_shouldRecreateSwap = true;
}

void VulkanEngine::setWindowSize(const u32 width, const u32 height) const {
	glfwSetWindowSize(m_window, static_cast<i32>(width), static_cast<i32>(height));
	const auto newSize = getWindowSize();
	if (newSize.first != width || newSize.second != height) {
		Logger::warn("Unable to resize window to requested size");
	}
}

std::pair<u32, u32> VulkanEngine::getWindowSize() const {
	int width, height;
	glfwGetWindowSize(m_window, &width, &height);
	return { static_cast<u32>(width), static_cast<u32>(height) };
}

void VulkanEngine::initWindow() {
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	// glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	// center the window on the primary monitor
	const GLFWvidmode* videoMode = glfwGetVideoMode(glfwGetPrimaryMonitor());
	glfwWindowHint(GLFW_POSITION_X, (videoMode->width - static_cast<i32>(WINDOW_WIDTH)) / 2);
	glfwWindowHint(GLFW_POSITION_Y, (videoMode->height - static_cast<i32>(WINDOW_HEIGHT)) / 2);
	m_window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Vulkan Renderer", nullptr, nullptr);

	glfwSetWindowUserPointer(m_window, this);
	glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* window, i32, i32) -> void {
		static_cast<VulkanEngine*>(glfwGetWindowUserPointer(window))->queueSwapRecreation();
	});
}

void VulkanEngine::initVulkan() {
	Logger::info("Initialising Vulkan");
	createInstance();
	setupDebugMessenger();
	createSurface();
	pickPhysicalDevice();
	createLogicalDevice();
	createSwapChain();
	createCommandPool();
	createCommandBuffers();
	createQueryPool();
	createDescriptorPool();
	createSyncObjects();
}

void VulkanEngine::initECS() {
	ECS::init();
	m_debugWindow = std::make_unique<DebugWindow>(this, m_window, m_instance, m_physicalDevice, m_device, m_graphicsQueue);
	m_assetManager = std::make_unique<AssetManager>(this);

	InputManager::setWindow(m_window);

	ECS::registerComponent<Transform>();
	ECS::registerComponent<Model3D>();
	ECS::registerComponent<HierarchyComponent>();
	ECS::registerComponent<NamedComponent>();
	ECS::registerComponent<ControlledCamera>();
	ECS::registerComponent<PointLight>();

	ECS::registerSystem<AllEntities>();
	ECS::setSystemSignature<AllEntities>(ECS::createSignature<>());

	ECS::registerSystem<EntitySearcher>();
	ECS::setSystemSignature<EntitySearcher>(ECS::createSignature<NamedComponent>());

	m_updatables.push_back(ECS::registerSystem<ControlledCameraSystem>(m_window));
	ECS::setSystemSignature<ControlledCameraSystem>(ECS::createSignature<ControlledCamera>());

	m_renderer = ECS::registerSystem<Renderer3D>(this, m_swapExtent);
	ECS::setSystemSignature<Renderer3D>(ECS::createSignature<Transform, Model3D>());

	m_updatables.push_back(ECS::registerSystem<TransformSystem>());
	ECS::setSystemSignature<TransformSystem>(ECS::createSignature<Transform>());

	ECS::registerSystem<LightSystem>();
	ECS::setSystemSignature<LightSystem>(ECS::createSignature<Transform, PointLight>());
}

void VulkanEngine::createScene() const {
	// m_assetManager->load("assets");

	ECS::Entity sphere = m_assetManager->loadGLB("assets/icosphere.glb");
	ECS::addComponent<PointLight>(sphere, {
		.colour = glm::vec3(1.0f, 1.0f, 1.0f),
		.strength = 200.0f
	});
	auto &sphereTransform = ECS::getComponent<Transform>(sphere);
	sphereTransform.scale = glm::vec3(0.1f);
	sphereTransform.position = glm::vec3(0.0f, 10.0f, 0.0f);

	const ECS::Entity map = m_assetManager->loadGLB("assets/cs_office.glb");
	ECS::getComponent<Transform>(map).scale = glm::vec3(0.01f);
}

void VulkanEngine::createInstance() {
	// check validation layer support if enabled
	if (ENABLE_VALIDATION_LAYERS && !checkValidationLayerSupport()) {
		throw std::runtime_error("Validation layers requested but not available");
	}

	constexpr vk::ApplicationInfo appInfo{
		.pApplicationName = "Vulkan Renderer",
		.applicationVersion = vk::makeApiVersion(1, 0, 0, 0),
		.pEngineName = "No Engine",
		.engineVersion = vk::makeApiVersion(1, 0, 0, 0),
		.apiVersion = vk::ApiVersion14
	};

	auto extensions = getRequiredExtensions();
	Logger::info("Loaded extensions: " + listify(extensions));
	vk::InstanceCreateInfo createInfo{
		.pApplicationInfo = &appInfo,
		.enabledLayerCount = 0,
		.enabledExtensionCount = static_cast<u32>(extensions.size()),
		.ppEnabledExtensionNames = extensions.data(),
	};

	if (ENABLE_VALIDATION_LAYERS) {
		createInfo.enabledLayerCount = static_cast<u32>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}

	m_instance = vk::raii::Instance(m_context, createInfo);
}

void VulkanEngine::createLogicalDevice() {
	QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);
	std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
	std::set uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

	float queuePriority = 1.0f;
	for (u32 queueFamily : uniqueQueueFamilies) {
		vk::DeviceQueueCreateInfo queueCreateInfo{
			.queueFamilyIndex = queueFamily,
			.queueCount = 1,
			.pQueuePriorities = &queuePriority,
		};
		queueCreateInfos.push_back(queueCreateInfo);
	}

	vk::PhysicalDeviceFeatures deviceFeatures = {
		.fillModeNonSolid = vk::True,
		.samplerAnisotropy = vk::True
	};
	vk::StructureChain createInfo = {
		vk::DeviceCreateInfo {
			.queueCreateInfoCount = static_cast<u32>(queueCreateInfos.size()),
			.pQueueCreateInfos = queueCreateInfos.data(),
			.enabledExtensionCount = static_cast<u32>(deviceExtensions.size()),
			.ppEnabledExtensionNames = deviceExtensions.data(),
			.pEnabledFeatures = &deviceFeatures,
		},
		vk::PhysicalDeviceDynamicRenderingFeatures {
			.dynamicRendering = vk::True
		}
	};

	m_device = vk::raii::Device(m_physicalDevice, createInfo.get<vk::DeviceCreateInfo>());
	m_graphicsQueue = vk::raii::Queue(m_device, indices.graphicsFamily.value(), 0);
	m_presentQueue = vk::raii::Queue(m_device, indices.presentFamily.value(), 0);
}

void VulkanEngine::createSurface() {
	VkSurfaceKHR cSurface;
	// use c bindings to create surface as c++ bindings not supported by glfw
	if (glfwCreateWindowSurface(*m_instance, m_window, nullptr, &cSurface) != VkResult::VK_SUCCESS)
		throw std::runtime_error("Failed to create window surface");

	// convert the returned c surface into vulkan_hpp bindings one
	m_surface = vk::raii::SurfaceKHR(m_instance, cSurface);
}

void VulkanEngine::createSwapChain() {
	vk::SurfaceCapabilitiesKHR capabilities = m_physicalDevice.getSurfaceCapabilitiesKHR(m_surface);
	m_swapExtent = chooseExtent(capabilities);
	u32 imageCount = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
		imageCount = capabilities.maxImageCount;
	}

	vk::SwapchainCreateInfoKHR createInfo = {
		.surface = m_surface,
		.minImageCount = imageCount,
		.imageFormat = getSwapColourFormat(),
		.imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear,
		.imageExtent = m_swapExtent,
		.imageArrayLayers = 1,
		.imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
		.preTransform = capabilities.currentTransform,
		.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
		.presentMode = m_presentMode,
		.clipped = vk::True,
		.oldSwapchain = m_swapChain,
	};

	QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);
	u32 queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };
	if (indices.graphicsFamily != indices.presentFamily) {
		createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else {
		createInfo.imageSharingMode = vk::SharingMode::eExclusive;
	}

	m_swapChain = vk::raii::SwapchainKHR(m_device, createInfo);

	// create image views
	for (const auto& image : m_swapChain.getImages()) {
		m_swapImageViews.push_back(createImageView(image, getSwapColourFormat(), vk::ImageAspectFlagBits::eColor));
	}
}

void VulkanEngine::createCommandPool() {
	QueueFamilyIndices queueFamilyIndices = findQueueFamilies(m_physicalDevice);
	const vk::CommandPoolCreateInfo createInfo{
		.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value()
	};
	m_commandPool = vk::raii::CommandPool(m_device, createInfo);
}

void VulkanEngine::createCommandBuffers() {
	vk::CommandBufferAllocateInfo allocInfo{
		.commandPool = m_commandPool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1,
	};

	m_commandBuffer = std::move(m_device.allocateCommandBuffers(allocInfo).front());
}

void VulkanEngine::createSyncObjects() {
	m_imageAvailableSemaphore = vk::raii::Semaphore(m_device, vk::SemaphoreCreateInfo{});
	m_inFlightFence = vk::raii::Fence(m_device, vk::FenceCreateInfo{ .flags = vk::FenceCreateFlagBits::eSignaled });

	for (size_t i = 0; i < m_swapImageViews.size(); ++i) {
		m_renderFinishedSemaphores.emplace_back(m_device, vk::SemaphoreCreateInfo{});
	}
}

void VulkanEngine::createQueryPool() {
	// check for device support
	vk::PhysicalDeviceLimits limits = m_physicalDevice.getProperties().limits;
	if (limits.timestampPeriod == 0 || !limits.timestampComputeAndGraphics) {
		throw std::runtime_error("Current GPU does not support timestamping");
	}

	vk::QueryPoolCreateInfo createInfo = {
		.queryType = vk::QueryType::eTimestamp,
		.queryCount = 2,
	};

	m_queryPool = vk::raii::QueryPool(m_device, createInfo);
}

void VulkanEngine::createDescriptorPool() {
	std::array poolSizes = {
		vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer, 64},
		vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler, 64},
		vk::DescriptorPoolSize{vk::DescriptorType::eUniformBufferDynamic, 64}
	};
	vk::DescriptorPoolCreateInfo poolInfo = {
		.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
		.maxSets = 256,
		.poolSizeCount = static_cast<u32>(poolSizes.size()),
		.pPoolSizes = poolSizes.data(),
	};
	m_descriptorPool = m_device.createDescriptorPool(poolInfo);
}

std::vector<const char*> VulkanEngine::getRequiredExtensions() {
	u32 glfwExtensionCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	for (auto ext : instanceExtensions) {
		extensions.push_back(ext);
	}

	if (ENABLE_VALIDATION_LAYERS) {
		extensions.push_back(vk::EXTDebugUtilsExtensionName);
	}

	return extensions;
}

bool VulkanEngine::checkValidationLayerSupport() {
	auto layers = vk::enumerateInstanceLayerProperties();

	for (const char* layer : validationLayers) {
		bool found = false;

		for (const auto& layerProps : layers) {
			if (strcmp(layer, layerProps.layerName) == 0) {
				found = true;
				break;
			}
		}
		if (!found) return false;
	}

	return true;
}

void VulkanEngine::setupDebugMessenger() {
	if constexpr (!ENABLE_VALIDATION_LAYERS) return;
	vk::DebugUtilsMessengerCreateInfoEXT createInfo{
		.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
		.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
		.pfnUserCallback = &debugCallback,
		.pUserData = nullptr
	};

	m_debugMessenger = m_instance.createDebugUtilsMessengerEXT(createInfo);
}

void VulkanEngine::pickPhysicalDevice() {
	auto devices = m_instance.enumeratePhysicalDevices();
	if (devices.empty())
		throw std::runtime_error("Failed to find GPUs with Vulkan support");
	m_physicalDevice = devices[0];
}

bool VulkanEngine::checkDeviceExtensionSupport(const vk::raii::PhysicalDevice& device) {
	auto extensions = device.enumerateDeviceExtensionProperties();
	std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
	for (const auto& extension : extensions) {
		requiredExtensions.erase(extension.extensionName);
	}
	return requiredExtensions.empty();
}

QueueFamilyIndices VulkanEngine::findQueueFamilies(const vk::raii::PhysicalDevice& device) const {
	QueueFamilyIndices indices;
	auto queueFamilies = device.getQueueFamilyProperties();
	int i = 0;
	for (const auto& queueFamily : queueFamilies) {
		// check graphics support
		if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) {
			indices.graphicsFamily = i;

		}
		// check window surface support
		if (device.getSurfaceSupportKHR(i, m_surface)) {
			indices.presentFamily = i;
		}
		// early break if we're done
		if (indices.isComplete()) break;
		// increment queue family index
		i++;
	}
	return indices;
}

vk::Extent2D VulkanEngine::chooseExtent(const vk::SurfaceCapabilitiesKHR& capabilities) const {
	if (capabilities.currentExtent.width != std::numeric_limits<u32>::max()) {
		return capabilities.currentExtent;
	}

	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height);

	vk::Extent2D extent = { static_cast<u32>(width), static_cast<u32>(height) };
	extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
	extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

	return extent;
}

vk::raii::ImageView VulkanEngine::createImageView(vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags) const {
	vk::ImageViewCreateInfo createInfo = {
		.image = image,
		.viewType = vk::ImageViewType::e2D,
		.format = format,
		.subresourceRange = {
			.aspectMask = aspectFlags,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		}
	};

	return vk::raii::ImageView(m_device, createInfo);
}

void VulkanEngine::copyBuffer(vk::Buffer src, vk::Buffer dst, vk::DeviceSize size) const {
	vk::raii::CommandBuffer commandBuffer = beginSingleCommand();

	vk::BufferCopy copyRegion{.size = size};
	commandBuffer.copyBuffer(src, dst, copyRegion);

	endSingleCommand(commandBuffer);
}

void VulkanEngine::transitionImageLayout(const vk::Image &image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) const {
	vk::PipelineStageFlags srcStage;
	vk::PipelineStageFlags dstStage;
	vk::AccessFlags srcAccess;
	vk::AccessFlags dstAccess;

	if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
		srcAccess = {};
		dstAccess = vk::AccessFlagBits::eTransferWrite;
		srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
		dstStage = vk::PipelineStageFlagBits::eTransfer;
	}
	else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
		srcAccess = vk::AccessFlagBits::eTransferWrite;
		dstAccess = vk::AccessFlagBits::eShaderRead;
		srcStage = vk::PipelineStageFlagBits::eTransfer;
		dstStage = vk::PipelineStageFlagBits::eFragmentShader;
	}
	else if (oldLayout == vk::ImageLayout::eColorAttachmentOptimal && newLayout == vk::ImageLayout::ePresentSrcKHR) {
		srcAccess = vk::AccessFlagBits::eColorAttachmentWrite;
		dstAccess = {};
		srcStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		dstStage = vk::PipelineStageFlagBits::eBottomOfPipe;
	}
	else if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eColorAttachmentOptimal) {
		srcAccess = {};
		dstAccess = vk::AccessFlagBits::eColorAttachmentWrite;
		srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
		dstStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	}
	else {
		throw std::invalid_argument("Layout transition not supported");
	}

	vk::raii::CommandBuffer commandBuffer = beginSingleCommand();

	vk::ImageMemoryBarrier barrier = {
		.srcAccessMask = srcAccess,
		.dstAccessMask = dstAccess,
		.oldLayout = oldLayout,
		.newLayout = newLayout,
		.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
		.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
		.image = image,
		.subresourceRange = {
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		}
	};
	commandBuffer.pipelineBarrier(srcStage, dstStage, {}, nullptr, nullptr, barrier);

	endSingleCommand(commandBuffer);
}

void VulkanEngine::copyBufferToImage(const vk::raii::Buffer &buffer, const vk::raii::Image &image, u32 width, u32 height) const {
	vk::raii::CommandBuffer commandBuffer = beginSingleCommand();

	vk::BufferImageCopy region = {
		.bufferOffset = 0,
		.bufferRowLength = 0,
		.bufferImageHeight = 0,
		.imageSubresource = {
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.mipLevel = 0,
			.baseArrayLayer = 0,
			.layerCount = 1
		},
		.imageOffset = {0, 0, 0},
		.imageExtent = {width, height, 1}
	};

	commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, region);

	endSingleCommand(commandBuffer);
}

std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> VulkanEngine::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties) const {
	std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> result = {nullptr, nullptr};
	// create buffer
	vk::BufferCreateInfo createInfo{
		.size = size,
		.usage = usage,
		.sharingMode = vk::SharingMode::eExclusive
	};
	result.first = vk::raii::Buffer(m_device, createInfo);

	// allocate memory
	vk::MemoryRequirements memReqs = result.first.getMemoryRequirements();
	vk::MemoryAllocateInfo allocInfo = {
		.allocationSize = memReqs.size,
		.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, properties)
	};
	result.second = m_device.allocateMemory(allocInfo);

	// bind memory
	result.first.bindMemory(result.second, 0);
	return result;
}

std::pair<vk::raii::Image, vk::raii::DeviceMemory> VulkanEngine::createImage(u32 width, u32 height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties) const {
	std::pair<vk::raii::Image, vk::raii::DeviceMemory> result = {nullptr, nullptr};

	vk::ImageCreateInfo imageInfo = {
		.imageType = vk::ImageType::e2D,
		.format = format,
		.extent = {
			.width = width,
			.height = height,
			.depth = 1
		},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = vk::SampleCountFlagBits::e1,
		.tiling = tiling,
		.usage = usage,
		.sharingMode = vk::SharingMode::eExclusive,
		.initialLayout = vk::ImageLayout::eUndefined
	};

	result.first = vk::raii::Image(m_device, imageInfo);

	vk::MemoryRequirements memoryRequirements = result.first.getMemoryRequirements();
	vk::MemoryAllocateInfo allocInfo = {
		.allocationSize = memoryRequirements.size,
		.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits, properties)
	};

	result.second = m_device.allocateMemory(allocInfo);
	result.first.bindMemory(result.second, 0);

	return result;
;}

u32 VulkanEngine::findMemoryType(u32 typeFilter, vk::MemoryPropertyFlags properties) const {
	auto const memProperties = m_physicalDevice.getMemoryProperties();
	for (u32 i = 0; i < memProperties.memoryTypeCount; i++) {
		if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("Failed to find suitable memory type!");
}

vk::raii::CommandBuffer VulkanEngine::beginSingleCommand() const {
	vk::CommandBufferAllocateInfo allocInfo = {
		.commandPool = m_commandPool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1
	};

	vk::raii::CommandBuffer commandBuffer = std::move(m_device.allocateCommandBuffers(allocInfo).front());

	vk::CommandBufferBeginInfo beginInfo{
		.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
	};

	commandBuffer.begin(beginInfo);

	return commandBuffer;
}

void VulkanEngine::endSingleCommand(const vk::raii::CommandBuffer &commandBuffer) const {
	commandBuffer.end();
	vk::SubmitInfo submitInfo = {
		.commandBufferCount = 1,
		.pCommandBuffers = &*commandBuffer
	};
	m_graphicsQueue.submit(submitInfo);
	m_graphicsQueue.waitIdle();
}

void VulkanEngine::mainLoop() {
	auto prevTime = static_cast<float>(glfwGetTime());
	while (!glfwWindowShouldClose(m_window)) {
		InputManager::update();
		glfwPollEvents();

		const auto updateStartTime = std::chrono::high_resolution_clock::now();
		for (const auto x : m_updatables) {
			x->update(m_deltaTime);
		}
		const auto updateEndTime = std::chrono::high_resolution_clock::now();
		m_cpuTime = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(updateEndTime - updateStartTime).count());

		drawFrame();

		const auto nowTime = static_cast<float>(glfwGetTime());
		m_deltaTime = nowTime - prevTime;
		prevTime = nowTime;
	}
	m_device.waitIdle();
}

void VulkanEngine::drawFrame() {
	while (m_device.waitForFences(*m_inFlightFence, vk::True, std::numeric_limits<u64>::max()) == vk::Result::eTimeout) {}

	auto [result, imageIndex] = m_swapChain.acquireNextImage(std::numeric_limits<u64>::max(), *m_imageAvailableSemaphore, nullptr);
	if (result == vk::Result::eErrorOutOfDateKHR) {
		recreateSwapChain();
		return;
	}
	m_device.resetFences(*m_inFlightFence);
	m_commandBuffer.reset();
	recordCommandBuffer(m_commandBuffer, imageIndex);

	vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
	const vk::SubmitInfo submitInfo = {
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &*m_imageAvailableSemaphore,
		.pWaitDstStageMask = waitStages,
		.commandBufferCount = 1,
		.pCommandBuffers = &*m_commandBuffer,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &*m_renderFinishedSemaphores[imageIndex],
	};

	m_graphicsQueue.submit(submitInfo, m_inFlightFence);

	const vk::PresentInfoKHR presentInfo = {
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &*m_renderFinishedSemaphores[imageIndex],
		.swapchainCount = 1,
		.pSwapchains = &*m_swapChain,
		.pImageIndices = &imageIndex,
	};
	try {
		vk::Result presentResult = m_presentQueue.presentKHR(presentInfo);
		if (presentResult == vk::Result::eErrorOutOfDateKHR || m_shouldRecreateSwap) throw vk::OutOfDateKHRError("");
	}
	catch (vk::OutOfDateKHRError&) {
		recreateSwapChain();
	}
}

void VulkanEngine::recordCommandBuffer(const vk::raii::CommandBuffer& commandBuffer, const u32 imageIndex) const {
	commandBuffer.begin({});
	commandBuffer.resetQueryPool(m_queryPool, 0, 2);
	commandBuffer.writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe, m_queryPool, 0);
	m_renderer->render(commandBuffer, m_swapChain.getImages().at(imageIndex), m_swapImageViews[imageIndex]);
	commandBuffer.writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, m_queryPool, 1);

	commandBuffer.end();
}

void VulkanEngine::recreateSwapChain() {
	m_shouldRecreateSwap = false;
	// check for minimization
	i32 width, height;
	glfwGetFramebufferSize(m_window, &width, &height);
	while (width == 0 || height == 0) {
		glfwGetFramebufferSize(m_window, &width, &height);
		glfwWaitEvents();
	}

	m_device.waitIdle();

	m_swapImageViews.clear();

	createSwapChain();
	m_renderer->setExtent(m_swapExtent);

	Logger::info("Recreated swapchain [{}x{}]", m_swapExtent.width, m_swapExtent.height);
}

void VulkanEngine::cleanup() const {
	glfwDestroyWindow(m_window);
	glfwTerminate();
	ECS::destroy();
}

int main() {
	VULKAN_HPP_DEFAULT_DISPATCHER.init();
	try {
		VulkanEngine app;
		app.run();
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}