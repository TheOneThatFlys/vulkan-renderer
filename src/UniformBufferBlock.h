#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "Common.h"
#include "VulkanEngine.h"

class VulkanEngine;

template <typename T>
class UniformBufferBlock {
public:
	UniformBufferBlock(const VulkanEngine* engine, u32 binding) : m_engine(engine), m_binding(binding) {
		vk::DeviceSize bufferSize = sizeof(T);
		std::tie(m_buffer, m_deviceMemory) = engine->createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible);
		m_data = static_cast<T*>(m_deviceMemory.mapMemory(0, bufferSize));
	}

	~UniformBufferBlock() {
		m_deviceMemory.unmapMemory();
	}

	void addToSet(const vk::raii::DescriptorSet& descriptorSet) const {
		vk::DescriptorBufferInfo bufferInfo = {
			.buffer = m_buffer,
			.offset = 0,
			.range = sizeof(T),
		};

		vk::WriteDescriptorSet writeInfo = {
			.dstSet = *descriptorSet,
			.dstBinding = m_binding,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eUniformBuffer,
			.pBufferInfo = &bufferInfo,
		};
		m_engine->getDevice().updateDescriptorSets(writeInfo, nullptr);
	}

	void setData(const T& data) {
		std::memcpy(m_data, &data, sizeof(T));
	}

private:
	vk::raii::Buffer m_buffer = nullptr;
	vk::raii::DeviceMemory m_deviceMemory = nullptr;
	T* m_data;

	const VulkanEngine* m_engine;
	u32 m_binding;
};

template <typename T>
class DynamicUniformBufferBlock {
public:
	DynamicUniformBufferBlock(const VulkanEngine* engine, u32 binding, u32 size = 256) : m_engine(engine), m_binding(binding), m_size(size) {
		// calculate aligned size
		u32 minAlignment = static_cast<u32>(engine->getPhysicalDevice().getProperties().limits.minUniformBufferOffsetAlignment);
		m_alignedItemSize = static_cast<u32>(sizeof(T) + minAlignment - 1) & ~(minAlignment - 1); // calculate the dynamic alignment size - https://github.com/SaschaWillems/Vulkan/tree/master/examples/dynamicuniformbuffer
		vk::DeviceSize bufferSize = m_alignedItemSize * m_size;
		std::tie(m_buffer, m_deviceMemory) = engine->createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible);
		m_data = static_cast<char*>(m_deviceMemory.mapMemory(0, bufferSize));
	}

	~DynamicUniformBufferBlock() {
		m_deviceMemory.unmapMemory();
	}

	void addToSet(const vk::raii::DescriptorSet& descriptorSet) const {
		vk::DescriptorBufferInfo bufferInfo = {
			.buffer = m_buffer,
			.offset = 0,
			.range = sizeof(T),
		};

		vk::WriteDescriptorSet writeInfo = {
			.dstSet = *descriptorSet,
			.dstBinding = m_binding,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eUniformBufferDynamic,
			.pBufferInfo = &bufferInfo,
		};
		m_engine->getDevice().updateDescriptorSets(writeInfo, nullptr);
	}

	void setData(u32 index, const T& data) {
		std::memcpy(m_data + index * m_alignedItemSize, &data, m_alignedItemSize);
	}

	u32 getItemSize() const {
		return m_alignedItemSize;
	}

private:
	vk::raii::Buffer m_buffer = nullptr;
	vk::raii::DeviceMemory m_deviceMemory = nullptr;
	char* m_data;

	const VulkanEngine* m_engine;
	u32 m_binding;
	u32 m_size; // maximum number of elements
	u32 m_alignedItemSize; // size of each element + padding
};
