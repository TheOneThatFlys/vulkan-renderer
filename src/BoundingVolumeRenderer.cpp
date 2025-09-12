#include "BoundingVolumeRenderer.h"

BoundingVolumeRenderer::BoundingVolumeRenderer(VulkanEngine* engine)
	: m_engine(engine)
	, m_frameUniforms(engine, 0)
	, m_modelUniforms(engine, 0)
{
	m_pipeline = Pipeline::Builder(engine)
		.addShaderStage("shaders/line.vert.spv")
		.addShaderStage("shaders/line.frag.spv")
		.setVertexInfo(BasicVertex::getBindingDescription(), BasicVertex::getAttributeDescriptions())
		.addBinding(0, 0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex) // view / project
		.addBinding(2, 0, vk::DescriptorType::eUniformBufferDynamic, vk::ShaderStageFlagBits::eVertex) // model data
		.setTopology(vk::PrimitiveTopology::eLineList)
		.create();

	m_frameDescriptor = m_pipeline->createDescriptorSet(FRAME_SET_NUMBER);
	m_modelDescriptor = m_pipeline->createDescriptorSet(MODEL_SET_NUMBER);

	m_frameUniforms.addToSet(m_frameDescriptor);
	m_modelUniforms.addToSet(m_modelDescriptor);

	createVolumes();
}

void BoundingVolumeRenderer::draw(const vk::raii::CommandBuffer& commandBuffer) {
	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline->getPipeline());

	const auto camera = ECS::getSystem<ControlledCameraSystem>();
	m_frameUniforms.setData({
		.view = camera->getViewMatrix(),
		.projection = camera->getProjectionMatrix(),
	});
	commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline->getLayout(), FRAME_SET_NUMBER, {*m_frameDescriptor}, {});

	u32 i = 0;
	for (const auto& sphere : m_sphereQueue) {
		m_modelUniforms.setData(i, {
			.transform = glm::scale(glm::translate(glm::mat4(1.0f), sphere.sphere.center), glm::vec3(sphere.sphere.radius)),
			.colour = sphere.colour
		});
		commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline->getLayout(), MODEL_SET_NUMBER, {*m_modelDescriptor}, {i * m_modelUniforms.getItemSize()});
		m_sphereMesh->draw(commandBuffer);
		++i;
	}

	m_sphereQueue.clear();
}

void BoundingVolumeRenderer::queueSphere(const Sphere &sphere, const glm::vec3 &colour) {
	m_sphereQueue.emplace_back(sphere, colour);
}

void BoundingVolumeRenderer::createVolumes() {
	// sphere
	constexpr u32 steps = 32;
	constexpr float dTheta = glm::radians(360.0f / static_cast<float>(steps));

	std::vector<BasicVertex> vertices;
	std::vector<u32> indexes;
	u32 index = 0;
	for (i32 axis = 0; axis < 3; ++axis) {
		const i32 otherAxis = (axis + 1) % 3;
		for (u32 i = 0; i < steps; ++i) {
			const float a = std::cos(dTheta * static_cast<float>(i));
			const float b = std::sin(dTheta * static_cast<float>(i));
			auto position = glm::vec3(0.0f);
			position[axis] = a;
			position[otherAxis] = b;
			vertices.emplace_back(position);
			indexes.emplace_back(index);
			++index;
			indexes.emplace_back(index);
		}
		indexes.pop_back();
		indexes.push_back(index - steps);
	}

	m_sphereMesh = std::make_unique<Mesh<BasicVertex>>(m_engine, vertices, indexes);
}