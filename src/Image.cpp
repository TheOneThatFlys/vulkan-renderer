#include "Image.h"

#include "VulkanEngine.h"

Image::Image(const ImageCreateInfo &info) : m_aspect(info.aspect) {
	const vk::ImageCreateInfo imageInfo = {
		.flags = info.flags,
		.imageType = info.imageType,
		.format = info.format,
		.extent = {
			.width = info.width,
			.height = info.height,
			.depth = info.depth
		},
		.mipLevels = info.mips,
		.arrayLayers = info.arrayLayers,
		.samples = info.samples,
		.tiling = info.tiling,
		.usage = info.usage,
		.sharingMode = info.sharingMode,
		.initialLayout = vk::ImageLayout::eUndefined
	};

	m_image = vk::raii::Image(VulkanEngine::getDevice(), imageInfo);
	m_imageMemory = VulkanEngine::allocateMemory(m_image.getMemoryRequirements(), info.properties);
	m_image.bindMemory(m_imageMemory, 0);

	const vk::ImageViewCreateInfo imageViewInfo = {
		.image = m_image,
		.viewType = info.viewType,
		.format = info.format,
		.subresourceRange = {
			.aspectMask = m_aspect,
			.baseMipLevel = 0,
			.levelCount = info.mips,
			.baseArrayLayer = 0,
			.layerCount = info.arrayLayers
		}
	};
	m_imageView = vk::raii::ImageView(VulkanEngine::getDevice(), imageViewInfo);
}

void Image::changeLayout(const vk::raii::CommandBuffer &commandBuffer, const ImageTransitionInfo &info) const {
	changeLayout(commandBuffer, m_image, {
		.oldLayout = info.oldLayout,
		.newLayout = info.newLayout,
		.mips = info.mips,
		.arrayLayers = info.arrayLayers,
		.aspect = m_aspect
	});
}

void Image::changeLayout(const vk::raii::CommandBuffer &commandBuffer, const vk::Image image, const ImageTransitionInfo &info) {
	vk::PipelineStageFlags srcStage;
	vk::PipelineStageFlags dstStage;
	vk::AccessFlags srcAccess;
	vk::AccessFlags dstAccess;

	if (info.oldLayout == vk::ImageLayout::eUndefined && info.newLayout == vk::ImageLayout::eTransferDstOptimal) {
		srcAccess = {};
		dstAccess = vk::AccessFlagBits::eTransferWrite;
		srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
		dstStage = vk::PipelineStageFlagBits::eTransfer;
	}
	else if (info.oldLayout == vk::ImageLayout::eTransferDstOptimal && info.newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
		srcAccess = vk::AccessFlagBits::eTransferWrite;
		dstAccess = vk::AccessFlagBits::eShaderRead;
		srcStage = vk::PipelineStageFlagBits::eTransfer;
		dstStage = vk::PipelineStageFlagBits::eFragmentShader;
	}
	else if (info.oldLayout == vk::ImageLayout::eColorAttachmentOptimal && info.newLayout == vk::ImageLayout::ePresentSrcKHR) {
		srcAccess = vk::AccessFlagBits::eColorAttachmentWrite;
		dstAccess = {};
		srcStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		dstStage = vk::PipelineStageFlagBits::eBottomOfPipe;
	}
	else if (info.oldLayout == vk::ImageLayout::eUndefined && info.newLayout == vk::ImageLayout::eColorAttachmentOptimal) {
		srcAccess = {};
		dstAccess = vk::AccessFlagBits::eColorAttachmentWrite;
		srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
		dstStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	}
	else if (info.oldLayout == vk::ImageLayout::eColorAttachmentOptimal && info.newLayout == vk::ImageLayout::eTransferSrcOptimal) {
		srcAccess = vk::AccessFlagBits::eColorAttachmentWrite;
		dstAccess = vk::AccessFlagBits::eTransferRead;
		srcStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		dstStage = vk::PipelineStageFlagBits::eTransfer;
	}
	else {
		throw std::invalid_argument("Layout transition not supported");
	}

	const vk::ImageMemoryBarrier barrier = {
		.srcAccessMask = srcAccess,
		.dstAccessMask = dstAccess,
		.oldLayout = info.oldLayout,
		.newLayout = info.newLayout,
		.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
		.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
		.image = image,
		.subresourceRange = {
			.aspectMask = info.aspect,
			.baseMipLevel = 0,
			.levelCount = info.mips,
			.baseArrayLayer = 0,
			.layerCount = info.arrayLayers
		}
	};
	commandBuffer.pipelineBarrier(srcStage, dstStage, {}, nullptr, nullptr, barrier);
}

const vk::raii::Image & Image::getImage() const {
    return m_image;
}

const vk::raii::DeviceMemory & Image::getMemory() const  {
    return m_imageMemory;
}

const vk::raii::ImageView & Image::getView() const {
    return m_imageView;
}
