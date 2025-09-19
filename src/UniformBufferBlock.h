#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "Common.h"
#include "VulkanEngine.h"

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

template <typename T, u64 S>
class UniformBufferBlockArray {
public:
	UniformBufferBlockArray(const VulkanEngine* engine, u32 binding) : m_engine(engine), m_binding(binding) {
		vk::DeviceSize bufferSize = sizeof(T) * S;
		std::tie(m_buffer, m_deviceMemory) = engine->createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible);
		m_data = static_cast<T*>(m_deviceMemory.mapMemory(0, bufferSize));
	}

	~UniformBufferBlockArray() {
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
			.descriptorCount = S,
			.descriptorType = vk::DescriptorType::eUniformBuffer,
			.pBufferInfo = &bufferInfo,
		};
		m_engine->getDevice().updateDescriptorSets(writeInfo, nullptr);
	}

	void setData(const std::array<T, S>& data) {
		std::memcpy(m_data, &data, sizeof(T)*S);
	}

	void setData(u64 index, const T& data) {
		std::memcpy(m_data + index, &data, sizeof(T));
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
	DynamicUniformBufferBlock(const VulkanEngine* engine, const u32 binding, const u32 size = 256) : m_engine(engine), m_binding(binding), m_size(size), m_alignedItemSize(calculateAlignment()) {
		createBuffers();
	}

	~DynamicUniformBufferBlock() {
		m_deviceMemory.unmapMemory();
	}

	// resize the buffer to newSize count elements - this will delete any data stored in the buffer
	void resize(const u32 newSize) {
		m_size = newSize;
		m_deviceMemory.unmapMemory();
		createBuffers();
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
		assert(index < m_size && "Index out of range");
		std::memcpy(m_data + index * m_alignedItemSize, &data, m_alignedItemSize);
	}

	u32 getItemSize() const {
		return m_alignedItemSize;
	}

	u32 getSize() const {
		return m_size;
	}

private:
	u32 calculateAlignment() const {
		const u32 minAlignment = static_cast<u32>(m_engine->getPhysicalDevice().getProperties().limits.minUniformBufferOffsetAlignment);
		return static_cast<u32>(sizeof(T) + minAlignment - 1) & ~(minAlignment - 1); // calculate the dynamic alignment size - https://github.com/SaschaWillems/Vulkan/tree/master/examples/dynamicuniformbuffer
	}

	void createBuffers() {
		const vk::DeviceSize bufferSize = m_alignedItemSize * m_size;
		std::tie(m_buffer, m_deviceMemory) = m_engine->createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible);
		m_data = static_cast<char*>(m_deviceMemory.mapMemory(0, bufferSize));
	}

	vk::raii::Buffer m_buffer = nullptr;
	vk::raii::DeviceMemory m_deviceMemory = nullptr;
	char* m_data = nullptr;

	const VulkanEngine* m_engine;
	u32 m_binding;
	u32 m_size; // maximum number of elements
	u32 m_alignedItemSize; // size of each element + padding
};
