#include "VulkanRenderer.h"

#include <stdexcept>
#include <cstdlib>
#include <memory>
#include <set>
#include <algorithm>
#include <fstream>
#include <string>

#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan_hpp_macros.hpp>

#include "InputManager.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

const std::vector<Vertex> vertices = {
	{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
	{{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
	{{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
	{{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}}
};
const std::vector<u16> indices = {
	0, 1, 2, 2, 3, 0
};

static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
	const vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
	vk::DebugUtilsMessageTypeFlagsEXT type,
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

vk::VertexInputBindingDescription Vertex::getBindingDescription() {
	return {
		.binding = 0,
		.stride = sizeof(Vertex),
		.inputRate = vk::VertexInputRate::eVertex
	};
}

std::array<vk::VertexInputAttributeDescription, 2> Vertex::getAttributeDescriptions() {
	std::array<vk::VertexInputAttributeDescription, 2> descriptions;
	// position
	descriptions[0] = {
		.location = 0,
		.binding = 0,
		.format = vk::Format::eR32G32B32Sfloat,
		.offset = offsetof(Vertex, pos)
	};
	// color
	descriptions[1] = {
		.location = 1,
		.binding = 0,
		.format = vk::Format::eR32G32B32Sfloat,
		.offset = offsetof(Vertex, color),
	};
	return descriptions;
}

void VulkanRenderer::run() {
	initWindow();
	initVulkan();
	m_camera = std::make_unique<CameraController>(m_window);
	InputManager::create(m_window);
	mainLoop();
	cleanup();
}

void VulkanRenderer::initWindow() {
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	m_window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Vulkan Renderer", nullptr, nullptr);
}

void VulkanRenderer::initVulkan() {
	LOG_INFO("Initialising Vulkan");
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
	createFramebuffers();
	createCommandPool();
	createCommandBuffers();
	createVertexBuffer();
	createIndexBuffer();
	createUniformBuffers();
	createDescriptorPool();
	createDescriptorSets();
	createSyncObjects();
}

void VulkanRenderer::createInstance() {
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
	LOG_INFO("Loaded extensions: " + listify(extensions));
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

void VulkanRenderer::createLogicalDevice() {
	QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);
	std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
	std::set<u32> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

	float queuePriority = 1.0f;
	for (u32 queueFamily : uniqueQueueFamilies) {
		vk::DeviceQueueCreateInfo queueCreateInfo{
			.queueFamilyIndex = queueFamily,
			.queueCount = 1,
			.pQueuePriorities = &queuePriority,
		};
		queueCreateInfos.push_back(queueCreateInfo);
	}

	vk::PhysicalDeviceFeatures deviceFeatures{};
	vk::DeviceCreateInfo deviceCreateInfo{
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

void VulkanRenderer::createSurface() {
	VkSurfaceKHR cSurface;
	// use c bindings to create surface as c++ bindings not supported by glfw
	if (glfwCreateWindowSurface(*m_instance, m_window, nullptr, &cSurface) != VkResult::VK_SUCCESS)
		throw std::runtime_error("Failed to create window surface");

	// convert the returned c surface into vulkan_hpp bindings one
	m_surface = vk::raii::SurfaceKHR(m_instance, cSurface);
}

void VulkanRenderer::createImageViews() {
	vk::ImageViewCreateInfo createInfo{
		.viewType = vk::ImageViewType::e2D,
		.format = vk::Format::eB8G8R8A8Srgb,

		.components{
			.r = vk::ComponentSwizzle::eIdentity,
			.g = vk::ComponentSwizzle::eIdentity,
			.b = vk::ComponentSwizzle::eIdentity,
			.a = vk::ComponentSwizzle::eIdentity,
		},

		.subresourceRange{
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	for (const auto& image : m_swapChain.getImages()) {
		createInfo.image = image;
		m_imageViews.emplace_back(m_device, createInfo);
	}
}

void VulkanRenderer::createFramebuffers() {
	for (const auto & imageView : m_imageViews) {
		vk::ImageView attachments = *imageView;
		vk::FramebufferCreateInfo framebufferInfo{
			.renderPass = m_renderPass,
			.attachmentCount = 1,
			.pAttachments = &attachments,
			.width = m_swapExtent.width,
			.height = m_swapExtent.height,
			.layers = 1
		};

		m_framebuffers.emplace_back(m_device, framebufferInfo);
	}
}

void VulkanRenderer::createRenderPass() {
	vk::AttachmentDescription colorAttachment{
		.format = vk::Format::eB8G8R8A8Srgb,
		.samples = vk::SampleCountFlagBits::e1,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
		.stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
		.initialLayout = vk::ImageLayout::eUndefined,
		.finalLayout = vk::ImageLayout::ePresentSrcKHR,
	};
	vk::AttachmentReference colorAttachmentRef{
		.attachment = 0,
		.layout = vk::ImageLayout::eColorAttachmentOptimal
	};
	vk::SubpassDescription subpass{
		.pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAttachmentRef
	};
	vk::SubpassDependency dependency{
		.srcSubpass = vk::SubpassExternal,
		.dstSubpass = 0,
		.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
		.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
		.srcAccessMask = vk::AccessFlagBits::eNone,
		.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
	};
	vk::RenderPassCreateInfo renderPassInfo{
		.attachmentCount = 1,
		.pAttachments = &colorAttachment,
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = 1,
		.pDependencies = &dependency,
	};
	m_renderPass = vk::raii::RenderPass(m_device, renderPassInfo);

}

void VulkanRenderer::createSwapChain() {
	vk::SurfaceCapabilitiesKHR capabilities = m_physicalDevice.getSurfaceCapabilitiesKHR(m_surface);
	m_swapExtent = chooseExtent(capabilities);
	u32 imageCount = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
		imageCount = capabilities.maxImageCount;
	}

	vk::SwapchainCreateInfoKHR createInfo{
		.surface = m_surface,
		.minImageCount = imageCount,
		.imageFormat = vk::Format::eB8G8R8A8Srgb,
		.imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear,
		.imageExtent = m_swapExtent,
		.imageArrayLayers = 1,
		.imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
		.preTransform = capabilities.currentTransform,
		.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
		.presentMode = vk::PresentModeKHR::eMailbox,
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

void VulkanRenderer::createGraphicsPipeline() {
	vk::raii::ShaderModule vertShader = createShaderModule(readFile("shaders/shader.vert.spv"));
	vk::raii::ShaderModule fragShader = createShaderModule(readFile("shaders/shader.frag.spv"));

	vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
		.stage = vk::ShaderStageFlagBits::eVertex,
		.module = vertShader,
		.pName = "main"
	};

	vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
		.stage = vk::ShaderStageFlagBits::eFragment,
		.module = fragShader,
		.pName = "main"
	};

	vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	auto bindingDescription = Vertex::getBindingDescription();
	auto attributeDescriptions = Vertex::getAttributeDescriptions();
	vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &bindingDescription,
		.vertexAttributeDescriptionCount = static_cast<u32>(attributeDescriptions.size()),
		.pVertexAttributeDescriptions = attributeDescriptions.data()
	};

	vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
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

	vk::PipelineRasterizationStateCreateInfo rasterizer{
		.depthClampEnable = vk::False,
		.rasterizerDiscardEnable = vk::False,
		.polygonMode = vk::PolygonMode::eFill,
		.cullMode = vk::CullModeFlagBits::eBack,
		.frontFace = vk::FrontFace::eCounterClockwise,
		.depthBiasEnable = vk::False,
		.lineWidth = 1.0f,
	};

	vk::PipelineMultisampleStateCreateInfo multisampling{
		.rasterizationSamples = vk::SampleCountFlagBits::e1,
		.sampleShadingEnable = vk::False,
	};

	vk::PipelineColorBlendAttachmentState colorBlendAttachment{
		.blendEnable = vk::False,
		.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
	};

	vk::PipelineColorBlendStateCreateInfo colorBlending{
		.logicOpEnable = vk::False,
		.attachmentCount = 1,
		.pAttachments = &colorBlendAttachment
	};

	vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
		.setLayoutCount = 1,
		.pSetLayouts = &*m_descriptorSetLayout,
	};
	m_pipelineLayout = vk::raii::PipelineLayout(m_device, pipelineLayoutInfo);

	vk::GraphicsPipelineCreateInfo pipelineInfo{
		.stageCount = 2,
		.pStages = shaderStages,
		.pVertexInputState = &vertexInputInfo,
		.pInputAssemblyState = &inputAssembly,
		.pViewportState = &viewportState,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pDepthStencilState = nullptr,
		.pColorBlendState = &colorBlending,
		.pDynamicState = &dynamicState,
		.layout = m_pipelineLayout,
		.renderPass = m_renderPass,
		.subpass = 0
	};

	m_pipeline = vk::raii::Pipeline(m_device, nullptr, pipelineInfo);
}

void VulkanRenderer::createCommandPool() {
	QueueFamilyIndices queueFamilyIndices = findQueueFamilies(m_physicalDevice);
	const vk::CommandPoolCreateInfo createInfo{
		.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value()
	};
	m_commandPool = vk::raii::CommandPool(m_device, createInfo);
}

void VulkanRenderer::createCommandBuffers() {
	vk::CommandBufferAllocateInfo allocInfo{
		.commandPool = m_commandPool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = MAX_FRAMES_IN_FLIGHT,
	};

	m_commandBuffers = m_device.allocateCommandBuffers(allocInfo);
}

void VulkanRenderer::createSyncObjects() {
	for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		m_imageAvailableSemaphores.emplace_back(m_device, vk::SemaphoreCreateInfo{});
		m_inFlightFences.emplace_back(m_device, vk::FenceCreateInfo{ .flags = vk::FenceCreateFlagBits::eSignaled });
	}
	for (size_t i = 0; i < m_imageViews.size(); ++i) {
		m_renderFinishedSemaphores.emplace_back(m_device, vk::SemaphoreCreateInfo{});
	}
}

void VulkanRenderer::createVertexBuffer() {
	const u64 bufferSize = sizeof(Vertex) * vertices.size();

	auto [stagingBuffer, stagingBufferMemory] = createBuffer(
		bufferSize,
		vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostCoherent |  vk::MemoryPropertyFlagBits::eHostVisible
	);

	void* data = stagingBufferMemory.mapMemory(0, bufferSize, {});
	memcpy(data, vertices.data(), bufferSize);
	stagingBufferMemory.unmapMemory();

	std::tie(m_vertexBuffer, m_vertexBufferMemory) = createBuffer(
		bufferSize,
		vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
		vk::MemoryPropertyFlagBits::eHostCoherent |  vk::MemoryPropertyFlagBits::eHostVisible
	);

	copyBuffer(stagingBuffer, m_vertexBuffer, bufferSize);
}

void VulkanRenderer::createIndexBuffer() {
	const u64 bufferSize = sizeof(u16) * indices.size();

	auto [stagingBuffer, stagingBufferMemory] = createBuffer(
		bufferSize,
		vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostCoherent |  vk::MemoryPropertyFlagBits::eHostVisible
	);

	void* data = stagingBufferMemory.mapMemory(0, bufferSize, {});
	memcpy(data, indices.data(), bufferSize);
	stagingBufferMemory.unmapMemory();

	std::tie(m_indexBuffer, m_indexBufferMemory) = createBuffer(
		bufferSize,
		vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
		vk::MemoryPropertyFlagBits::eHostCoherent |  vk::MemoryPropertyFlagBits::eHostVisible
	);

	copyBuffer(stagingBuffer, m_indexBuffer, bufferSize);
}

void VulkanRenderer::createDescriptorSetLayout() {
	vk::DescriptorSetLayoutBinding layoutBinding{
		.binding = 0,
		.descriptorType = vk::DescriptorType::eUniformBuffer,
		.descriptorCount = 1,
		.stageFlags = vk::ShaderStageFlagBits::eVertex
	};
	vk::DescriptorSetLayoutCreateInfo layoutInfo{
		.bindingCount = 1,
		.pBindings = &layoutBinding
	};
	m_descriptorSetLayout = m_device.createDescriptorSetLayout(layoutInfo);
}

void VulkanRenderer::createUniformBuffers() {
	vk::DeviceSize bufferSize = sizeof(MatrixUniforms);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		auto [buffer, bufferMemory] = createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible);
		m_uniformBuffers.emplace_back(std::move(buffer));
		m_uniformBuffersMemory.emplace_back(std::move(bufferMemory));
		m_uniformBuffersMapped.push_back(m_uniformBuffersMemory[i].mapMemory(0, bufferSize));
	}
}

void VulkanRenderer::createDescriptorPool() {
	vk::DescriptorPoolSize poolSize = {
		.type = vk::DescriptorType::eUniformBuffer,
		.descriptorCount = MAX_FRAMES_IN_FLIGHT,
	};
	vk::DescriptorPoolCreateInfo poolInfo = {
		.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
		.maxSets = MAX_FRAMES_IN_FLIGHT,
		.poolSizeCount = 1,
		.pPoolSizes = &poolSize,
	};
	m_descriptorPool = m_device.createDescriptorPool(poolInfo);
}

void VulkanRenderer::createDescriptorSets() {
	std::vector layouts(MAX_FRAMES_IN_FLIGHT, *m_descriptorSetLayout);
	vk::DescriptorSetAllocateInfo allocInfo = {
		.descriptorPool = m_descriptorPool,
		.descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
		.pSetLayouts = layouts.data()
	};
	m_descriptorSets = m_device.allocateDescriptorSets(allocInfo);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vk::DescriptorBufferInfo bufferInfo{
			.buffer = m_uniformBuffers[i],
			.offset = 0,
			.range = sizeof(MatrixUniforms),
		};
		vk::WriteDescriptorSet descriptorWrite = {
			.dstSet = m_descriptorSets[i],
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eUniformBuffer,
			.pBufferInfo = &bufferInfo,
		};
		m_device.updateDescriptorSets(descriptorWrite, nullptr);
	}
}

vk::raii::ShaderModule VulkanRenderer::createShaderModule(const std::vector<char>& code) const {
	vk::ShaderModuleCreateInfo createInfo{
		.codeSize = code.size() * sizeof(char),
		.pCode = reinterpret_cast<const u32*>(code.data())
	};
	return vk::raii::ShaderModule(m_device, createInfo);
}

std::vector<const char*> VulkanRenderer::getRequiredExtensions() {
	u32 glfwExtensionCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	if (ENABLE_VALIDATION_LAYERS) {
		extensions.push_back(vk::EXTDebugUtilsExtensionName);
	}

	return extensions;
}

bool VulkanRenderer::checkValidationLayerSupport() {
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

void VulkanRenderer::setupDebugMessenger() {
	if constexpr (!ENABLE_VALIDATION_LAYERS) return;
	vk::DebugUtilsMessengerCreateInfoEXT createInfo{
		.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
		.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
		.pfnUserCallback = &debugCallback,
		.pUserData = nullptr
	};

	m_debugMessenger = m_instance.createDebugUtilsMessengerEXT(createInfo);
}

void VulkanRenderer::pickPhysicalDevice() {
	auto devices = m_instance.enumeratePhysicalDevices();
	if (devices.empty())
		throw std::runtime_error("Failed to find GPUs with Vulkan support");
	m_physicalDevice = devices[0];
}

bool VulkanRenderer::checkDeviceExtensionSupport(const vk::raii::PhysicalDevice& device) {
	auto extensions = device.enumerateDeviceExtensionProperties();
	std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
	for (const auto& extension : extensions) {
		requiredExtensions.erase(extension.extensionName);
	}
	return requiredExtensions.empty();
}

QueueFamilyIndices VulkanRenderer::findQueueFamilies(const vk::raii::PhysicalDevice& device) const {
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

vk::Extent2D VulkanRenderer::chooseExtent(const vk::SurfaceCapabilitiesKHR& capabilities) const {
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

void VulkanRenderer::recordCommandBuffer(const vk::raii::CommandBuffer& commandBuffer, const u32 imageIndex) const {
	vk::CommandBufferBeginInfo beginInfo;
	commandBuffer.begin(beginInfo);
	vk::ClearValue clearColor = { {0.0f, 0.0f, 0.0f, 1.0f} };
	vk::RenderPassBeginInfo renderPassInfo{
		.renderPass = m_renderPass,
		.framebuffer = m_framebuffers[imageIndex],
		.renderArea{
			.offset = {0, 0},
			.extent = m_swapExtent,
		},
		.clearValueCount = 1,
		.pClearValues = &clearColor,
	};
	commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline);
	vk::Viewport viewport{
		.x = 0.0f,
		.y = 0.0f,
		.width = static_cast<float>(m_swapExtent.width),
		.height = static_cast<float>(m_swapExtent.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f
	};
	vk::Rect2D scissor{
		.offset = {0, 0},
		.extent = m_swapExtent
	};
	commandBuffer.setViewport(0, { viewport });
	commandBuffer.setScissor(0, { scissor });

	commandBuffer.bindVertexBuffers(0, *m_vertexBuffer, {0});
	commandBuffer.bindIndexBuffer(m_indexBuffer, 0, vk::IndexType::eUint16);

	commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, *m_descriptorSets[m_currentFrame], nullptr);

	commandBuffer.drawIndexed(static_cast<u32>(indices.size()), 1, 0, 0, 0);
	commandBuffer.endRenderPass();
	commandBuffer.end();
}

std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> VulkanRenderer::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties) const {
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
	vk::MemoryAllocateInfo allocInfo{
		.allocationSize = memReqs.size,
		.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
	};
	result.second = m_device.allocateMemory(allocInfo);

	// bind memory
	result.first.bindMemory(result.second, 0);
	return result;
}

void VulkanRenderer::copyBuffer(vk::Buffer src, vk::Buffer dst, vk::DeviceSize size) const {
	// create a temporary command buffer to perform copy
	vk::CommandBufferAllocateInfo allocInfo{
		.commandPool = m_commandPool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1
	};
	vk::raii::CommandBuffer commandBuffer = std::move(m_device.allocateCommandBuffers(allocInfo).front());

	vk::CommandBufferBeginInfo beginInfo{
		.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
	};
	commandBuffer.begin(beginInfo);
	vk::BufferCopy copyRegion{.size = size};
	commandBuffer.copyBuffer(src, dst, copyRegion);
	commandBuffer.end();
	vk::SubmitInfo submitInfo{
		.commandBufferCount = 1,
		.pCommandBuffers = &*commandBuffer
	};
	m_graphicsQueue.submit(submitInfo);
	m_graphicsQueue.waitIdle();
}

u32 VulkanRenderer::findMemoryType(u32 typeFilter, vk::MemoryPropertyFlags properties) const {
	auto const memProperties = m_physicalDevice.getMemoryProperties();
	for (u32 i = 0; i < memProperties.memoryTypeCount; i++) {
		if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("Failed to find suitable memory type!");
}

void VulkanRenderer::mainLoop() {
	float prevTime = static_cast<float>(glfwGetTime());
	float deltaTime = 1.0f / 120;
	float updateTime = 0;
	size_t fpsPtr = 0;
	float movingFps[64] = {};
	while (!glfwWindowShouldClose(m_window)) {
		InputManager::update();
		glfwPollEvents();
		drawFrame();
		const float nowTime = static_cast<float>(glfwGetTime());
		deltaTime = nowTime - prevTime;
		m_camera->update(deltaTime);
		prevTime = nowTime;

		updateTime += deltaTime;

		movingFps[fpsPtr] = 1.0f / deltaTime;
		fpsPtr = (fpsPtr + 1) % 64;
		float avgFps = 0;
		for (float fps : movingFps) {
			avgFps += fps;
		}
		avgFps /= 64;
		if (updateTime > 0.2f) {
			glfwSetWindowTitle(m_window, std::string("Vulkan Renderer | FPS ").append(std::to_string(static_cast<int>(avgFps))).c_str());
			updateTime = 0.0f;
		}
	}
	m_device.waitIdle();
}

void VulkanRenderer::drawFrame() {
	// m_graphicsQueue.waitIdle();
	// wait indefinitely for fence
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

void VulkanRenderer::updateUniformBuffer(const u32 currentImage) const {
	MatrixUniforms ubo{
		.model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -2.0f)),
		.view = m_camera->getViewMatrix(),
		.projection = m_camera->getProjectionMatrix(),
	};
	memcpy(m_uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

void VulkanRenderer::cleanup() const {
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
		VulkanRenderer app;
		app.run();
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
