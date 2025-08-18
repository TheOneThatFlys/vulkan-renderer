#pragma once

#include <vector>
#include <optional>
#include <array>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "Common.h"
#include "CameraController.h"

constexpr uint32_t WINDOW_WIDTH = 1280;
constexpr uint32_t WINDOW_HEIGHT = 720;

constexpr u32 MAX_FRAMES_IN_FLIGHT = 2;

constexpr std::array validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

constexpr std::array<const char*, 0> instanceExtensions = {

};

constexpr std::array deviceExtensions = {
	vk::KHRSwapchainExtensionName,
	vk::EXTMemoryBudgetExtensionName,
};

#ifdef NDEBUG
constexpr bool ENABLE_VALIDATION_LAYERS = false;
#else
constexpr bool ENABLE_VALIDATION_LAYERS = true;
#endif

// Forward decl.
class DebugWindow;
class AssetManager;

struct QueueFamilyIndices {
	std::optional<u32> graphicsFamily;
	std::optional<u32> presentFamily;

	bool isComplete() const {
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

struct MatrixUniforms {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 projection;
};

class VulkanEngine {
public:
	void run();
	CameraController* getCamera() const;
	DebugWindow* getDebugWindow() const;
	FrameTimeInfo getFrameTimeInfo() const;
	VRAMUsageInfo getVramUsage() const;

	vk::raii::ImageView createImageView(vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags) const;
	std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties) const;
	std::pair<vk::raii::Image, vk::raii::DeviceMemory> createImage(u32 width, u32 height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties) const;
	void transitionImageLayout(vk::raii::Image& image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) const;
	void copyBufferToImage(vk::raii::Buffer& buffer, vk::raii::Image& image, u32 width, u32 height) const;
	void copyBuffer(vk::Buffer src, vk::Buffer dst, vk::DeviceSize size) const;

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
	void createQueryPool();
	void createDescriptorSetLayout();
	void createUniformBuffers();
	void createDescriptorPool();
	void createDescriptorSets();
	void createTextureImageView();
	void createTextureSampler();
	void createDepthResources();

	vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const;

	static bool checkValidationLayerSupport();
	static std::vector<const char*> getRequiredExtensions();
	void setupDebugMessenger();
	void pickPhysicalDevice();
	static bool checkDeviceExtensionSupport(const vk::raii::PhysicalDevice& device);
	QueueFamilyIndices findQueueFamilies(const vk::raii::PhysicalDevice& device) const;

	vk::Extent2D chooseExtent(const vk::SurfaceCapabilitiesKHR& capabilities) const;

	void createTextureImage(const char* path);
	u32 findMemoryType(u32 typeFilter, vk::MemoryPropertyFlags properties) const;

	vk::raii::CommandBuffer beginSingleCommand() const;
	void endSingleCommand(const vk::raii::CommandBuffer& commandBuffer) const;

	void mainLoop();
	void recordCommandBuffer(const vk::raii::CommandBuffer& commandBuffer, u32 imageIndex) const;
	void drawFrame();
	void updateUniformBuffer(u32 currentImage) const;

	void cleanup() const;

	float m_deltaTime = 1.0f / 120.0f;

	GLFWwindow* m_window = nullptr;

	std::unique_ptr<CameraController> m_camera = nullptr;

	vk::raii::Context m_context;
	vk::raii::Instance m_instance = nullptr;
	vk::raii::DebugUtilsMessengerEXT m_debugMessenger = nullptr;

	vk::raii::PhysicalDevice m_physicalDevice = nullptr;
	vk::raii::Device m_device = nullptr;

	vk::raii::Queue m_graphicsQueue = nullptr;
	vk::raii::Queue m_presentQueue = nullptr;

	vk::raii::SurfaceKHR m_surface = nullptr;
	vk::raii::SwapchainKHR m_swapChain = nullptr;
	std::vector<vk::raii::ImageView> m_swapImageViews;
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

	vk::raii::QueryPool m_queryPool = nullptr;

	std::vector<vk::raii::Buffer> m_uniformBuffers;
	std::vector<vk::raii::DeviceMemory> m_uniformBuffersMemory;
	std::vector<void*> m_uniformBuffersMapped;

	vk::raii::DescriptorPool m_descriptorPool = nullptr;
	std::vector<vk::raii::DescriptorSet> m_descriptorSets;

	vk::raii::Image m_textureImage = nullptr;
	vk::raii::DeviceMemory m_textureImageMemory = nullptr;
	vk::raii::ImageView m_textureImageView = nullptr;
	vk::raii::Sampler m_textureSampler = nullptr;

	vk::raii::Image m_depthImage = nullptr;
	vk::raii::DeviceMemory m_depthImageMemory = nullptr;
	vk::raii::ImageView m_depthImageView = nullptr;

	std::unique_ptr<AssetManager> m_assetManager = nullptr;
	std::unique_ptr<DebugWindow> m_debugWindow;
};

