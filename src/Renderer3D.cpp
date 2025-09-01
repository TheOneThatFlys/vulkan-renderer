#include "Renderer3D.h"

#include <glm/gtc/matrix_inverse.hpp>

#include "Components.h"
#include "DebugWindow.h"

Renderer3D::Renderer3D(VulkanEngine *engine, vk::Extent2D extent)
    : m_engine(engine)
    , m_frameUniforms(m_engine, 0)
    , m_modelUniforms(m_engine, 0)
    , m_fragFrameUniforms(m_engine, 1)
	, m_extent(extent)
{
	createPipeline();
	createDepthBuffer();

	m_frameDescriptor = m_pipeline->createDescriptorSet(FRAME_SET_NUMBER);
	m_modelDescriptor = m_pipeline->createDescriptorSet(MODEL_SET_NUMBER);

    m_frameUniforms.addToSet(m_frameDescriptor);
    m_fragFrameUniforms.addToSet(m_frameDescriptor);
    m_modelUniforms.addToSet(m_modelDescriptor);
    // create camera
    m_camera = ECS::createEntity();
    ECS::addComponent<ControlledCamera>(m_camera, {});
    ECS::addComponent<NamedComponent>(m_camera, {"Camera"});
}

void Renderer3D::render(const vk::raii::CommandBuffer &commandBuffer, const vk::Image& image, const vk::ImageView& imageView) {

	m_engine->transitionImageLayout(image, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);

	vk::RenderingAttachmentInfo colourAttachment = {
		.imageView = imageView,
		.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.clearValue = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 0.0f)
	};
	vk::RenderingAttachmentInfo depthAttachment = {
		.imageView = m_depthBufferImage,
		.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eDontCare,
		.clearValue = vk::ClearDepthStencilValue(1.0f, 0)
	};
	vk::RenderingInfo renderingInfo = {
		.renderArea = {
			.offset = {0, 0},
			.extent = m_extent
		},
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colourAttachment,
		.pDepthAttachment = &depthAttachment
	};

	commandBuffer.beginRendering(renderingInfo);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline->getPipeline());
    const vk::Viewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(m_extent.width),
        .height = static_cast<float>(m_extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    const vk::Rect2D scissor = {
        .offset = {0, 0},
        .extent = m_extent
    };
    commandBuffer.setViewport(0, viewport);
    commandBuffer.setScissor(0, scissor);
	commandBuffer.setPolygonModeEXT(m_polygonMode);

    const auto camera = ECS::getSystem<ControlledCameraSystem>();
    m_frameUniforms.setData({
        .view = camera->getViewMatrix(),
        .projection = camera->getProjectionMatrix(),
    });

    u32 nLights;
    m_fragFrameUniforms.setData({
        .cameraPosition = ECS::getComponent<ControlledCamera>(m_camera).position,
        .lights = ECS::getSystem<LightSystem>()->getLights(nLights),
        .nLights = nLights
    });
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline->getLayout(), FRAME_SET_NUMBER, {*m_frameDescriptor}, nullptr);

    u32 i = 0;
    for (const ECS::Entity entity : m_entities) {
        auto& modelInfo = ECS::getComponent<Model3D>(entity);
        auto& modelTransform = ECS::getComponent<Transform>(entity);
        m_modelUniforms.setData(i, {modelTransform.transform, glm::mat4(glm::mat3(glm::inverseTranspose(modelTransform.transform)))});

        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline->getLayout(), MODEL_SET_NUMBER, {*m_modelDescriptor}, {i * m_modelUniforms.getItemSize()});

        modelInfo.material->use(commandBuffer, m_pipeline->getLayout());
        modelInfo.mesh->draw(commandBuffer);
        ++i;
    }

	m_engine->getDebugWindow()->draw(commandBuffer);

	commandBuffer.endRendering();

	m_engine->transitionImageLayout(image, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR);

}

const Pipeline * Renderer3D::getPipeline() const {
	return m_pipeline.get();
}

void Renderer3D::createPipeline() {
	m_pipeline = Pipeline::Builder(m_engine)
		.addShaderStage("shaders/shader.vert.spv")
        .addShaderStage("shaders/shader.frag.spv")
        .setVertexInfo(Vertex::getBindingDescription(), Vertex::getAttributeDescriptions())
		.addDynamicState(vk::DynamicState::ePolygonModeEXT)

        .addBinding(0, 0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex) // view / project
        .addBinding(0, 1, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eFragment) // frame data - lights & camera

        .addBinding(1, 0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment) // material - base
        .addBinding(1, 1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment) // material - mr
        .addBinding(1, 2, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment) // material - ao
        .addBinding(1, 3, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment) // material - normal

        .addBinding(2, 0, vk::DescriptorType::eUniformBufferDynamic, vk::ShaderStageFlagBits::eVertex) // model data
		.create();
}

void Renderer3D::createDepthBuffer() {
	std::tie(m_depthBuffer, m_depthBufferMemory) = m_engine->createImage(
		m_extent.width,
		m_extent.height,
		m_engine->getDepthFormat(),
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eDepthStencilAttachment,
		vk::MemoryPropertyFlagBits::eDeviceLocal
	);

	m_depthBufferImage = m_engine->createImageView(m_depthBuffer, m_engine->getDepthFormat(), vk::ImageAspectFlagBits::eDepth);
}