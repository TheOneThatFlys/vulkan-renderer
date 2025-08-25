#pragma once

#include <vector>
#include <optional>
#include <array>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "Common.h"
#include "ControlledCameraSystem.h"

constexpr u32 WINDOW_WIDTH = 1280;
constexpr u32 WINDOW_HEIGHT = 720;

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
class Pipeline;
class Renderer3D;

struct QueueFamilyIndices {
	std::optional<u32> graphicsFamily;
	std::optional<u32> presentFamily;

	bool isComplete() const {
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

class VulkanEngine {
public:
	void run();
	DebugWindow* getDebugWindow() const;
	FrameTimeInfo getFrameTimeInfo() const;
	VRAMUsageInfo getVramUsage() const;
	const vk::raii::Device& getDevice() const;
	const vk::raii::PhysicalDevice& getPhysicalDevice() const;
	const vk::raii::DescriptorPool& getDescriptorPool() const;
	const Pipeline* getPipeline() const;

	std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties) const;
	void copyBuffer(vk::Buffer src, vk::Buffer dst, vk::DeviceSize size) const;
	std::pair<vk::raii::Image, vk::raii::DeviceMemory> createImage(u32 width, u32 height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties) const;
	vk::raii::ImageView createImageView(vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags) const;
	void copyBufferToImage(const vk::raii::Buffer& buffer, const vk::raii::Image& image, u32 width, u32 height) const;
	void transitionImageLayout(const vk::raii::Image& image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) const;

private:
	void initWindow();
	void initVulkan();
	void initECS();

	void createInstance();
	void createLogicalDevice();
	void createSurface();
	void createImageViews();
	void createFramebuffers();
	void createRenderPass();
	void createGraphicsPipeline();
	void createSwapChain();
	void createCommandPool();
	void createCommandBuffers();
	void createSyncObjects();
	void createQueryPool();
	void createDescriptorPool();
	void createDepthResources();

	static bool checkValidationLayerSupport();
	static std::vector<const char*> getRequiredExtensions();
	void setupDebugMessenger();
	void pickPhysicalDevice();
	static bool checkDeviceExtensionSupport(const vk::raii::PhysicalDevice& device);
	QueueFamilyIndices findQueueFamilies(const vk::raii::PhysicalDevice& device) const;

	vk::Extent2D chooseExtent(const vk::SurfaceCapabilitiesKHR& capabilities) const;

	u32 findMemoryType(u32 typeFilter, vk::MemoryPropertyFlags properties) const;

	vk::raii::CommandBuffer beginSingleCommand() const;
	void endSingleCommand(const vk::raii::CommandBuffer& commandBuffer) const;

	void mainLoop();
	void recordCommandBuffer(const vk::raii::CommandBuffer& commandBuffer, u32 imageIndex) const;
	void drawFrame() const;

	void cleanup() const;

	float m_deltaTime = 1.0f / 120.0f;
	double m_cpuTime = 0.0f;

	Renderer3D* m_renderer = nullptr;
	std::vector<IUpdatable*> m_updatables;

	GLFWwindow* m_window = nullptr;

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

	std::unique_ptr<Pipeline> m_pipeline = nullptr;
	vk::raii::RenderPass m_renderPass = nullptr;

	vk::raii::CommandPool m_commandPool = nullptr;
	vk::raii::CommandBuffer m_commandBuffer = nullptr;

	vk::raii::Semaphore m_imageAvailableSemaphore = nullptr;
	std::vector<vk::raii::Semaphore> m_renderFinishedSemaphores;
	vk::raii::Fence m_inFlightFence = nullptr;

	vk::raii::QueryPool m_queryPool = nullptr;
	vk::raii::DescriptorPool m_descriptorPool = nullptr;

	vk::raii::Image m_depthImage = nullptr;
	vk::raii::DeviceMemory m_depthImageMemory = nullptr;
	vk::raii::ImageView m_depthImageView = nullptr;

	std::unique_ptr<AssetManager> m_assetManager = nullptr;
	std::unique_ptr<DebugWindow> m_debugWindow = nullptr;
};

