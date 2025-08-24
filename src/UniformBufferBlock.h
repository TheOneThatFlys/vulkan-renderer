#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "Common.h"
#include "VulkanEngine.h"

class VulkanEngine;

template <typename T>
class UniformBufferBlock {
public:
	UniformBufferBlock(const VulkanEngine* engine, u32 binding);
	~UniformBufferBlock();
	void addToSet(const vk::raii::DescriptorSet& descriptorSet) const;
	void setData(const T& data);

private:
	vk::raii::Buffer m_buffer = nullptr;
	vk::raii::DeviceMemory m_deviceMemory = nullptr;
	T* m_data;

	const VulkanEngine* m_engine;
	u32 m_binding;
};

template<typename T>
UniformBufferBlock<T>::UniformBufferBlock(const VulkanEngine *engine, u32 binding) : m_binding(binding), m_engine(engine) {
	vk::DeviceSize bufferSize = sizeof(T);
	std::tie(m_buffer, m_deviceMemory) = engine->createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible);
	m_data = static_cast<T*>(m_deviceMemory.mapMemory(0, bufferSize));
}

template<typename T>
UniformBufferBlock<T>::~UniformBufferBlock() {
	m_deviceMemory.unmapMemory();
}

template<typename T>
void UniformBufferBlock<T>::addToSet(const vk::raii::DescriptorSet& descriptorSet) const {
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

template<typename T>
void UniformBufferBlock<T>::setData(const T& data) {
	std::memcpy(m_data, &data, sizeof(T));
}

