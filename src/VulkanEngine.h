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
	vk::KHRDynamicRenderingExtensionName,
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
	static void run();
	static DebugWindow* getDebugWindow();
	static GLFWwindow* getWindow();
	FrameTimeInfo getFrameTimeInfo();
	VRAMUsageInfo getVramUsage() const;
	static const vk::raii::Instance& getInstance();
	static const vk::raii::Queue& getGraphicsQueue();
	static const vk::raii::Device& getDevice();
	static const vk::raii::PhysicalDevice& getPhysicalDevice();
	static const vk::raii::DescriptorPool& getDescriptorPool();
	static Renderer3D *getRenderer();
	static AssetManager* getAssetManager();
	static void setPresentMode(vk::PresentModeKHR mode);
	static vk::PresentModeKHR getPresentMode();

	static vk::Format getSwapColourFormat();
	static vk::Format getDepthFormat();

	static void queueSwapRecreation();
	static void queueRendererRebuild();
	static void setWindowSize(u32 width, u32 height);
	static std::pair<u32, u32> getWindowSize();

	static void addUpdateListener(IUpdatable* updatable);

	static std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties);
	static void copyBuffer(vk::Buffer src, vk::Buffer dst, vk::DeviceSize size);
	static vk::raii::DeviceMemory allocateMemory(const vk::MemoryRequirements& requirements, vk::MemoryPropertyFlags properties);

	static vk::raii::CommandBuffer beginSingleCommand();
	static void endSingleCommand(const vk::raii::CommandBuffer& commandBuffer);

	static VulkanEngine& get();

private:
	void initWindow();
	void initVulkan();
	void initECS();
	void createScene() const;

	void createInstance();
	void createLogicalDevice();
	void createSurface();
	void createSwapChain();
	void createCommandPool();
	void createCommandBuffers();
	void createSyncObjects();
	void createQueryPool();
	void createDescriptorPool();

	static bool checkValidationLayerSupport();
	static std::vector<const char*> getRequiredExtensions();
	void setupDebugMessenger();
	void pickPhysicalDevice();
	static bool checkDeviceExtensionSupport(const vk::raii::PhysicalDevice& device);
	QueueFamilyIndices findQueueFamilies(const vk::raii::PhysicalDevice& device) const;

	vk::Extent2D chooseExtent(const vk::SurfaceCapabilitiesKHR& capabilities) const;

	void mainLoop();
	void recordCommandBuffer(const vk::raii::CommandBuffer& commandBuffer, u32 imageIndex);
	void drawFrame();

	void recreateSwapChain();

	void cleanup() const;

	static VulkanEngine s_engine;

	float m_deltaTime = 1.0f / 120.0f;

	FrameTimeInfo m_timeInfo;

	vk::PresentModeKHR m_presentMode = vk::PresentModeKHR::eFifo;
	bool m_shouldRecreateSwap = false;
	bool m_shouldRebuildRenderer = false;

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
	vk::Extent2D m_swapExtent;

	vk::raii::CommandPool m_commandPool = nullptr;
	vk::raii::CommandBuffer m_commandBuffer = nullptr;

	vk::raii::Semaphore m_imageAvailableSemaphore = nullptr;
	std::vector<vk::raii::Semaphore> m_renderFinishedSemaphores;
	vk::raii::Fence m_inFlightFence = nullptr;

	vk::raii::QueryPool m_queryPool = nullptr;
	vk::raii::DescriptorPool m_descriptorPool = nullptr;

	std::unique_ptr<AssetManager> m_assetManager;
	std::unique_ptr<DebugWindow> m_debugWindow;
};

