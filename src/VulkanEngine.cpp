#include "VulkanEngine.h"

#include <stdexcept>
#include <cstdlib>
#include <memory>
#include <set>
#include <algorithm>
#include <fstream>
#include <string>

#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan_hpp_macros.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <iostream>
#include <stb_image.h>

#include "InputManager.h"
#include "DebugWindow.h"
#include "AssetManager.h"

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

static std::vector<char> readFile(const std::string& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);
	if (!file.is_open()) {
		throw std::runtime_error("Failed to open file: " + filename);
	}
	std::vector<char> buffer(file.tellg());
	file.seekg(0);
	file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
	file.close();
	return buffer;
}

void VulkanEngine::run() {
	initWindow();
	initVulkan();
	m_camera = std::make_unique<CameraController>(m_window);
	m_debugWindow = std::make_unique<DebugWindow>(this, m_window, m_instance, m_physicalDevice, m_device, m_graphicsQueue, m_renderPass);
	m_assetManager = std::make_unique<AssetManager>(this);
	m_assetManager->load("assets");
	InputManager::create(m_window);
	mainLoop();
	cleanup();
}

CameraController* VulkanEngine::getCamera() const {
	return m_camera.get();
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

void VulkanEngine::initWindow() {
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	// center the window on the primary monitor
	const GLFWvidmode* videoMode = glfwGetVideoMode(glfwGetPrimaryMonitor());
	glfwWindowHint(GLFW_POSITION_X, (videoMode->width - static_cast<i32>(WINDOW_WIDTH)) / 2);
	glfwWindowHint(GLFW_POSITION_Y, (videoMode->height - static_cast<i32>(WINDOW_HEIGHT)) / 2);
	m_window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Vulkan Renderer", nullptr, nullptr);
}

void VulkanEngine::initVulkan() {
	Logger::info("Initialising Vulkan");
	createInstance();
	setupDebugMessenger();
	createSurface();
	pickPhysicalDevice();
	createLogicalDevice();
	createSwapChain();
	createImageViews();
	createRenderPass();
	createDescriptorSetLayout();
	createGraphicsPipeline();
	createCommandPool();
	createCommandBuffers();
	createQueryPool();
	createUniformBuffers();
	createDepthResources();
	createFramebuffers();
	createTextureImage("assets/monkey.jpg");
	createTextureImageView();
	createTextureSampler();
	createDescriptorPool();
	createDescriptorSets();
	createSyncObjects();
}

void VulkanEngine::createInstance() {
	// check validation layer support if enabled
	if (ENABLE_VALIDATION_LAYERS && !checkValidationLayerSupport()) {
		throw std::runtime_error("Validation layers requested but not available");
	}

	constexpr vk::ApplicationInfo appInfo{
		.pApplicationName = "Vulkan Renderer",
		.applicationVersion = vk::makeVersion(1, 0, 0),
		.pEngineName = "No Engine",
		.engineVersion = vk::makeVersion(1, 0, 0),
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
		.samplerAnisotropy = vk::True
	};
	vk::DeviceCreateInfo deviceCreateInfo = {
		.queueCreateInfoCount = static_cast<u32>(queueCreateInfos.size()),
		.pQueueCreateInfos = queueCreateInfos.data(),
		.enabledExtensionCount = static_cast<u32>(deviceExtensions.size()),
		.ppEnabledExtensionNames = deviceExtensions.data(),
		.pEnabledFeatures = &deviceFeatures,
	};

	m_device = vk::raii::Device(m_physicalDevice, deviceCreateInfo);
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

void VulkanEngine::createImageViews() {
	for (const auto& image : m_swapChain.getImages()) {
		m_swapImageViews.push_back(createImageView(image, vk::Format::eB8G8R8A8Unorm, vk::ImageAspectFlagBits::eColor));
	}
}

void VulkanEngine::createFramebuffers() {
	for (const auto & imageView : m_swapImageViews) {
		std::array attachments = {*imageView, *m_depthImageView};
		vk::FramebufferCreateInfo framebufferInfo{
			.renderPass = m_renderPass,
			.attachmentCount = static_cast<u32>(attachments.size()),
			.pAttachments = attachments.data(),
			.width = m_swapExtent.width,
			.height = m_swapExtent.height,
			.layers = 1
		};

		m_framebuffers.emplace_back(m_device, framebufferInfo);
	}
}

void VulkanEngine::createRenderPass() {
	vk::AttachmentDescription colorAttachment = {
		.format = vk::Format::eB8G8R8A8Unorm,
		.samples = vk::SampleCountFlagBits::e1,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
		.stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
		.initialLayout = vk::ImageLayout::eUndefined,
		.finalLayout = vk::ImageLayout::ePresentSrcKHR,
	};
	vk::AttachmentReference colorAttachmentRef = {
		.attachment = 0,
		.layout = vk::ImageLayout::eColorAttachmentOptimal
	};
	vk::AttachmentDescription depthAttachment = {
		.format = vk::Format::eD32Sfloat,
		.samples = vk::SampleCountFlagBits::e1,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eDontCare,
		.stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
		.stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
		.initialLayout = vk::ImageLayout::eUndefined,
		.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal
	};
	vk::AttachmentReference depthAttachmentRef = {
		.attachment = 1,
		.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal
	};

	std::array attachments = {colorAttachment, depthAttachment};

	vk::SubpassDescription subpass = {
		.pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAttachmentRef,
		.pDepthStencilAttachment = &depthAttachmentRef
	};
	vk::SubpassDependency dependency = {
		.srcSubpass = vk::SubpassExternal,
		.dstSubpass = 0,
		.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eLateFragmentTests,
		.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
		.srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite,
		.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
	};
	vk::RenderPassCreateInfo renderPassInfo = {
		.attachmentCount = static_cast<u32>(attachments.size()),
		.pAttachments = attachments.data(),
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = 1,
		.pDependencies = &dependency,
	};
	m_renderPass = vk::raii::RenderPass(m_device, renderPassInfo);

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
		.imageFormat = vk::Format::eB8G8R8A8Unorm,
		.imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear,
		.imageExtent = m_swapExtent,
		.imageArrayLayers = 1,
		.imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
		.preTransform = capabilities.currentTransform,
		.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
		.presentMode = vk::PresentModeKHR::eFifo,
		.clipped = vk::True,
		.oldSwapchain = nullptr,
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
}

void VulkanEngine::createGraphicsPipeline() {
	vk::raii::ShaderModule vertShader = createShaderModule(readFile("shaders/shader.vert.spv"));
	vk::raii::ShaderModule fragShader = createShaderModule(readFile("shaders/shader.frag.spv"));

	vk::PipelineShaderStageCreateInfo vertShaderStageInfo = {
		.stage = vk::ShaderStageFlagBits::eVertex,
		.module = vertShader,
		.pName = "main"
	};

	vk::PipelineShaderStageCreateInfo fragShaderStageInfo = {
		.stage = vk::ShaderStageFlagBits::eFragment,
		.module = fragShader,
		.pName = "main"
	};

	vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	auto bindingDescription = Vertex::getBindingDescription();
	auto attributeDescriptions = Vertex::getAttributeDescriptions();
	vk::PipelineVertexInputStateCreateInfo vertexInputInfo = {
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &bindingDescription,
		.vertexAttributeDescriptionCount = static_cast<u32>(attributeDescriptions.size()),
		.pVertexAttributeDescriptions = attributeDescriptions.data()
	};

	vk::PipelineInputAssemblyStateCreateInfo inputAssembly = {
		.topology = vk::PrimitiveTopology::eTriangleList
	};

	std::vector dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
	vk::PipelineDynamicStateCreateInfo dynamicState{
		.dynamicStateCount = static_cast<u32>(dynamicStates.size()),
		.pDynamicStates = dynamicStates.data()
	};

	vk::PipelineViewportStateCreateInfo viewportState = {
		.viewportCount = 1,
		.scissorCount = 1,
	};

	vk::PipelineRasterizationStateCreateInfo rasterizer = {
		.depthClampEnable = vk::False,
		.rasterizerDiscardEnable = vk::False,
		.polygonMode = vk::PolygonMode::eFill,
		.cullMode = vk::CullModeFlagBits::eBack,
		.frontFace = vk::FrontFace::eCounterClockwise,
		.depthBiasEnable = vk::False,
		.lineWidth = 1.0f,
	};

	vk::PipelineMultisampleStateCreateInfo multisampling = {
		.rasterizationSamples = vk::SampleCountFlagBits::e1,
		.sampleShadingEnable = vk::False,
	};

	vk::PipelineDepthStencilStateCreateInfo depthStencil = {
		.depthTestEnable = vk::True,
		.depthWriteEnable = vk::True,
		.depthCompareOp = vk::CompareOp::eLess,
		.depthBoundsTestEnable = vk::False,
		.stencilTestEnable = vk::False
	};

	vk::PipelineColorBlendAttachmentState colorBlendAttachment = {
		.blendEnable = vk::False,
		.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
	};

	vk::PipelineColorBlendStateCreateInfo colorBlending = {
		.logicOpEnable = vk::False,
		.attachmentCount = 1,
		.pAttachments = &colorBlendAttachment
	};

	vk::PipelineLayoutCreateInfo pipelineLayoutInfo = {
		.setLayoutCount = 1,
		.pSetLayouts = &*m_descriptorSetLayout,
	};
	m_pipelineLayout = vk::raii::PipelineLayout(m_device, pipelineLayoutInfo);

	vk::GraphicsPipelineCreateInfo pipelineInfo = {
		.stageCount = 2,
		.pStages = shaderStages,
		.pVertexInputState = &vertexInputInfo,
		.pInputAssemblyState = &inputAssembly,
		.pViewportState = &viewportState,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pDepthStencilState = &depthStencil,
		.pColorBlendState = &colorBlending,
		.pDynamicState = &dynamicState,
		.layout = m_pipelineLayout,
		.renderPass = m_renderPass,
		.subpass = 0
	};

	m_pipeline = vk::raii::Pipeline(m_device, nullptr, pipelineInfo);
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
		.commandBufferCount = MAX_FRAMES_IN_FLIGHT,
	};

	m_commandBuffers = m_device.allocateCommandBuffers(allocInfo);
}

void VulkanEngine::createSyncObjects() {
	for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		m_imageAvailableSemaphores.emplace_back(m_device, vk::SemaphoreCreateInfo{});
		m_inFlightFences.emplace_back(m_device, vk::FenceCreateInfo{ .flags = vk::FenceCreateFlagBits::eSignaled });
	}
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

void VulkanEngine::createDescriptorSetLayout() {
	vk::DescriptorSetLayoutBinding uniformLayoutBinding = {
		.binding = 0,
		.descriptorType = vk::DescriptorType::eUniformBuffer,
		.descriptorCount = 1,
		.stageFlags = vk::ShaderStageFlagBits::eVertex
	};
	vk::DescriptorSetLayoutBinding samplerLayoutBinding = {
		.binding = 1,
		.descriptorType = vk::DescriptorType::eCombinedImageSampler,
		.descriptorCount = 1,
		.stageFlags = vk::ShaderStageFlagBits::eFragment
	};
	std::array bindings = {uniformLayoutBinding, samplerLayoutBinding};
	vk::DescriptorSetLayoutCreateInfo layoutInfo = {
		.bindingCount = static_cast<u32>(bindings.size()),
		.pBindings = bindings.data()
	};
	m_descriptorSetLayout = m_device.createDescriptorSetLayout(layoutInfo);
}

void VulkanEngine::createUniformBuffers() {
	vk::DeviceSize bufferSize = sizeof(MatrixUniforms);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		auto [buffer, bufferMemory] = createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible);
		m_uniformBuffers.emplace_back(std::move(buffer));
		m_uniformBuffersMemory.emplace_back(std::move(bufferMemory));
		m_uniformBuffersMapped.push_back(m_uniformBuffersMemory[i].mapMemory(0, bufferSize));
	}
}

void VulkanEngine::createDescriptorPool() {
	vk::DescriptorPoolSize uniformPoolSize = {
		.type = vk::DescriptorType::eUniformBuffer,
		.descriptorCount = MAX_FRAMES_IN_FLIGHT,
	};
	vk::DescriptorPoolSize samplerPoolSize = {
		.type = vk::DescriptorType::eCombinedImageSampler,
		.descriptorCount = MAX_FRAMES_IN_FLIGHT
	};
	std::array poolSizes = {uniformPoolSize, samplerPoolSize};
	vk::DescriptorPoolCreateInfo poolInfo = {
		.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
		.maxSets = MAX_FRAMES_IN_FLIGHT,
		.poolSizeCount = static_cast<u32>(poolSizes.size()),
		.pPoolSizes = poolSizes.data(),
	};
	m_descriptorPool = m_device.createDescriptorPool(poolInfo);
}

void VulkanEngine::createDescriptorSets() {
	std::vector layouts(MAX_FRAMES_IN_FLIGHT, *m_descriptorSetLayout);
	vk::DescriptorSetAllocateInfo allocInfo = {
		.descriptorPool = m_descriptorPool,
		.descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
		.pSetLayouts = layouts.data()
	};
	m_descriptorSets = m_device.allocateDescriptorSets(allocInfo);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vk::DescriptorBufferInfo bufferInfo = {
			.buffer = m_uniformBuffers[i],
			.offset = 0,
			.range = sizeof(MatrixUniforms),
		};
		vk::DescriptorImageInfo imageInfo = {
			.sampler = m_textureSampler,
			.imageView = m_textureImageView,
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
		};

		vk::WriteDescriptorSet uniformDescriptorWrite = {
			.dstSet = m_descriptorSets[i],
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eUniformBuffer,
			.pBufferInfo = &bufferInfo,
		};
		vk::WriteDescriptorSet samplerDescriptorWrite = {
			.dstSet = m_descriptorSets[i],
			.dstBinding = 1,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eCombinedImageSampler,
			.pImageInfo = &imageInfo
		};
		std::array descriptorWrites = {uniformDescriptorWrite, samplerDescriptorWrite};
		m_device.updateDescriptorSets(descriptorWrites, nullptr);
	}
}

void VulkanEngine::createTextureImageView() {
	m_textureImageView = createImageView(m_textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor);
}

void VulkanEngine::createTextureSampler() {
	vk::SamplerCreateInfo createInfo = {
		.magFilter = vk::Filter::eLinear,
		.minFilter = vk::Filter::eLinear,
		.mipmapMode = vk::SamplerMipmapMode::eLinear,
		.addressModeU = vk::SamplerAddressMode::eRepeat,
		.addressModeV = vk::SamplerAddressMode::eRepeat,
		.addressModeW = vk::SamplerAddressMode::eRepeat,
		.mipLodBias = 0.0f,
		.anisotropyEnable = vk::True,
		.maxAnisotropy = m_physicalDevice.getProperties().limits.maxSamplerAnisotropy,
		.compareEnable = vk::False,
		.compareOp = vk::CompareOp::eAlways,
		.minLod = 0.0f,
		.maxLod = 0.0f,
		.unnormalizedCoordinates = vk::False
	};

	m_textureSampler = vk::raii::Sampler(m_device, createInfo);
}

void VulkanEngine::createDepthResources() {
	std::tie(m_depthImage, m_depthImageMemory) = createImage(
		m_swapExtent.width,
		m_swapExtent.height,
		vk::Format::eD32Sfloat,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eDepthStencilAttachment,
		vk::MemoryPropertyFlagBits::eDeviceLocal
	);

	m_depthImageView = createImageView(m_depthImage, vk::Format::eD32Sfloat, vk::ImageAspectFlagBits::eDepth);
}

vk::raii::ShaderModule VulkanEngine::createShaderModule(const std::vector<char>& code) const {
	vk::ShaderModuleCreateInfo createInfo{
		.codeSize = code.size() * sizeof(char),
		.pCode = reinterpret_cast<const u32*>(code.data())
	};
	return vk::raii::ShaderModule(m_device, createInfo);
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

void VulkanEngine::createTextureImage(const char* path) {
	i32 width, height, channels;
	stbi_set_flip_vertically_on_load(true);
	stbi_uc* pixels = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);
	if (pixels == nullptr) throw std::runtime_error(std::format("Unable to load image at {} ({})", path, stbi_failure_reason()));
	vk::DeviceSize imageSize = width * height * 4;

	auto [stagingBuffer, stagingBufferMemory] = createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible);
	void* data = stagingBufferMemory.mapMemory(0, imageSize);
	memcpy(data, pixels, imageSize);
	stagingBufferMemory.unmapMemory();
	stbi_image_free(pixels);

	std::tie(m_textureImage, m_textureImageMemory) = createImage(
		width, height,
		vk::Format::eR8G8B8A8Srgb,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
		vk::MemoryPropertyFlagBits::eDeviceLocal
	);

	// transition image layout for optimal copying
	transitionImageLayout(m_textureImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
	// copy staging buffer data to image
	copyBufferToImage(stagingBuffer, m_textureImage, width, height);
	// transition to format optimal for shader reads
	transitionImageLayout(m_textureImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
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

void VulkanEngine::recordCommandBuffer(const vk::raii::CommandBuffer& commandBuffer, const u32 imageIndex) const {
	vk::CommandBufferBeginInfo beginInfo;
	commandBuffer.begin(beginInfo);
	commandBuffer.resetQueryPool(m_queryPool, 0, 2);
	commandBuffer.writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe, m_queryPool, 0);
	std::array<vk::ClearValue, 2> clearValues = {
		vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f),
		vk::ClearDepthStencilValue(1.0f, 0)
	};

	vk::RenderPassBeginInfo renderPassInfo = {
		.renderPass = m_renderPass,
		.framebuffer = m_framebuffers[imageIndex],
		.renderArea{
			.offset = {0, 0},
			.extent = m_swapExtent,
		},
		.clearValueCount = static_cast<u32>(clearValues.size()),
		.pClearValues = clearValues.data(),
	};
	commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline);
	vk::Viewport viewport = {
		.x = 0.0f,
		.y = 0.0f,
		.width = static_cast<float>(m_swapExtent.width),
		.height = static_cast<float>(m_swapExtent.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f
	};
	vk::Rect2D scissor = {
		.offset = {0, 0},
		.extent = m_swapExtent
	};
	commandBuffer.setViewport(0, { viewport });
	commandBuffer.setScissor(0, { scissor });

	commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, *m_descriptorSets[m_currentFrame], nullptr);

	for (const auto& mesh : m_assetManager->getMeshes()) {
		mesh.draw(commandBuffer);
	}

	m_debugWindow->draw(commandBuffer);

	commandBuffer.endRenderPass();

	commandBuffer.writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, m_queryPool, 1);

	commandBuffer.end();
}

void VulkanEngine::copyBuffer(vk::Buffer src, vk::Buffer dst, vk::DeviceSize size) const {
	vk::raii::CommandBuffer commandBuffer = beginSingleCommand();

	vk::BufferCopy copyRegion{.size = size};
	commandBuffer.copyBuffer(src, dst, copyRegion);

	endSingleCommand(commandBuffer);
}

void VulkanEngine::transitionImageLayout(vk::raii::Image &image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) const {
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

void VulkanEngine::copyBufferToImage(vk::raii::Buffer &buffer, vk::raii::Image &image, u32 width, u32 height) const {
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
	float prevTime = static_cast<float>(glfwGetTime());
	while (!glfwWindowShouldClose(m_window)) {
		InputManager::update();
		glfwPollEvents();
		drawFrame();
		const float nowTime = static_cast<float>(glfwGetTime());
		m_deltaTime = nowTime - prevTime;
		m_camera->update(m_deltaTime);
		prevTime = nowTime;
	}
	m_device.waitIdle();
}

void VulkanEngine::drawFrame() {
	// m_graphicsQueue.waitIdle();
	// wait indefinitely
	while (m_device.waitForFences(*m_inFlightFences[m_currentFrame], vk::True, std::numeric_limits<u64>::max()) == vk::Result::eTimeout) {}
	m_device.resetFences(*m_inFlightFences[m_currentFrame]);
	updateUniformBuffer(m_currentFrame);
	auto [result, imageIndex] = m_swapChain.acquireNextImage(std::numeric_limits<u64>::max(), *m_imageAvailableSemaphores[m_currentFrame], nullptr);
	m_commandBuffers[m_currentFrame].reset();
	recordCommandBuffer(m_commandBuffers[m_currentFrame], imageIndex);

	vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
	vk::SubmitInfo submitInfo{
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &*m_imageAvailableSemaphores[m_currentFrame],
		.pWaitDstStageMask = waitStages,
		.commandBufferCount = 1,
		.pCommandBuffers = &*m_commandBuffers[m_currentFrame],
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &*m_renderFinishedSemaphores[imageIndex],
	};

	m_graphicsQueue.submit(submitInfo, m_inFlightFences[m_currentFrame]);

	vk::PresentInfoKHR presentInfo{
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &*m_renderFinishedSemaphores[imageIndex],
		.swapchainCount = 1,
		.pSwapchains = &*m_swapChain,
		.pImageIndices = &imageIndex,
	};
	vk::Result presentResult = m_presentQueue.presentKHR(presentInfo);
	if (presentResult != vk::Result::eSuccess) throw std::runtime_error("Error presenting frame");

	m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanEngine::updateUniformBuffer(const u32 currentImage) const {
	MatrixUniforms ubo{
		.model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -2.0f)),
		.view = m_camera->getViewMatrix(),
		.projection = m_camera->getProjectionMatrix(),
	};
	memcpy(m_uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

void VulkanEngine::cleanup() const {
	for (auto const& buffer : m_uniformBuffersMemory) {
		buffer.unmapMemory();
	}

	glfwDestroyWindow(m_window);
	glfwTerminate();
}

int main()
{
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
