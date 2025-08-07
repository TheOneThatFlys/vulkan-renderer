#pragma once

#include <iostream>
#include <vector>
#include <optional>
#include <array>

#ifdef __INTELLISENSE__
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include "Common.h"
#include "CameraController.h"

constexpr uint32_t WINDOW_WIDTH = 1280;
constexpr uint32_t WINDOW_HEIGHT = 720;

constexpr u32 MAX_FRAMES_IN_FLIGHT = 2;

const std::vector validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

const std::vector deviceExtensions = {
	vk::KHRSwapchainExtensionName,
	vk::KHRSpirv14ExtensionName,
	vk::KHRShaderDrawParametersExtensionName,
};

#ifdef NDEBUG
constexpr bool ENABLE_VALIDATION_LAYERS = false;
#define LOG_INFO(s)
#else
constexpr bool ENABLE_VALIDATION_LAYERS = true;
#define LOG_INFO(s) std::cout << "[INFO] " << s << std::endl
#endif

struct QueueFamilyIndices {
	std::optional<u32> graphicsFamily;
	std::optional<u32> presentFamily;

	bool isComplete() const {
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

struct Vertex {
	glm::vec3 pos;
	glm::vec3 color;

	static vk::VertexInputBindingDescription getBindingDescription();

	static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions();
};

struct MatrixUniforms {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 projection;
};

class VulkanRenderer {
public:
	void run();


private:
	void initWindow();
	void initVulkan();

	void createInstance();
	void createLogicalDevice();
	void createSurface();
	void createImageViews();
	void createFramebuffers();
	void createRenderPass();
	void createSwapChain();
	void createGraphicsPipeline();
	void createCommandPool();
	void createCommandBuffers();
	void createSyncObjects();
	void createVertexBuffer();
	void createIndexBuffer();
	void createDescriptorSetLayout();
	void createUniformBuffers();
	void createDescriptorPool();
	void createDescriptorSets();
	vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const;

	static bool checkValidationLayerSupport();
	static std::vector<const char*> getRequiredExtensions();
	void setupDebugMessenger();
	void pickPhysicalDevice();
	static bool checkDeviceExtensionSupport(const vk::raii::PhysicalDevice& device);
	QueueFamilyIndices findQueueFamilies(const vk::raii::PhysicalDevice& device) const;

	vk::Extent2D chooseExtent(const vk::SurfaceCapabilitiesKHR& capabilities) const;

	std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties) const;
	void copyBuffer(vk::Buffer src, vk::Buffer dst, vk::DeviceSize size) const;
	u32 findMemoryType(u32 typeFilter, vk::MemoryPropertyFlags properties) const;

	void mainLoop();
	void recordCommandBuffer(const vk::raii::CommandBuffer& commandBuffer, u32 imageIndex) const;
	void drawFrame();
	void updateUniformBuffer(u32 currentImage) const;

	void cleanup() const;


	GLFWwindow* m_window = nullptr;

	std::unique_ptr<CameraController> m_camera;

	vk::raii::Context m_context;
	vk::raii::Instance m_instance = nullptr;
	vk::raii::DebugUtilsMessengerEXT m_debugMessenger = nullptr;

	vk::raii::PhysicalDevice m_physicalDevice = nullptr;
	vk::raii::Device m_device = nullptr;

	vk::raii::Queue m_graphicsQueue = nullptr;
	vk::raii::Queue m_presentQueue = nullptr;

	vk::raii::SurfaceKHR m_surface = nullptr;
	vk::raii::SwapchainKHR m_swapChain = nullptr;
	std::vector<vk::raii::ImageView> m_imageViews;
	std::vector<vk::raii::Framebuffer> m_framebuffers;
	vk::Extent2D m_swapExtent;

	vk::raii::DescriptorSetLayout m_descriptorSetLayout = nullptr;
	vk::raii::PipelineLayout m_pipelineLayout = nullptr;
	vk::raii::Pipeline m_pipeline = nullptr;
	vk::raii::RenderPass m_renderPass = nullptr;
	
	vk::raii::CommandPool m_commandPool = nullptr;
	std::vector<vk::raii::CommandBuffer> m_commandBuffers;

	std::vector<vk::raii::Semaphore> m_imageAvailableSemaphores;
	std::vector<vk::raii::Semaphore> m_renderFinishedSemaphores;
	std::vector<vk::raii::Fence> m_inFlightFences;
	u32 m_currentFrame = 0;

	vk::raii::Buffer m_vertexBuffer = nullptr;
	vk::raii::DeviceMemory m_vertexBufferMemory = nullptr;
	vk::raii::Buffer m_indexBuffer = nullptr;
	vk::raii::DeviceMemory m_indexBufferMemory = nullptr;

	std::vector<vk::raii::Buffer> m_uniformBuffers;
	std::vector<vk::raii::DeviceMemory> m_uniformBuffersMemory;
	std::vector<void*> m_uniformBuffersMapped;

	vk::raii::DescriptorPool m_descriptorPool = nullptr;
	std::vector<vk::raii::DescriptorSet> m_descriptorSets;
};
	