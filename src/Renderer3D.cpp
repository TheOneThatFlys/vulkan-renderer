#include "Renderer3D.h"

#include <chrono>

#include <glm/gtc/matrix_inverse.hpp>

#include "AssetManager.h"
#include "Components.h"
#include "DebugWindow.h"
#include "Skybox.h"

Renderer3D::Renderer3D(const vk::Extent2D extent)
    : m_extent(extent)
    , m_modelUniforms(ECS::MAX_ENTITIES)
	, m_boundingVolumeRenderer(std::make_unique<BoundingVolumeRenderer>(this))
	, m_modelSelector(std::make_unique<ModelSelector>(m_extent))
{
	createPipelines();
	createAttachments();

	m_frameDescriptor = m_pipeline->createDescriptorSet(FRAME_SET_NUMBER);
	m_modelDescriptor = m_pipeline->createDescriptorSet(MODEL_SET_NUMBER);
	m_skyboxDescriptor = m_skyboxPipeline->createDescriptorSet(FRAME_SET_NUMBER);

    m_frameUniforms.addToSet(m_frameDescriptor, 0);
	m_frameUniforms.addToSet(m_skyboxDescriptor, 0);
    m_fragFrameUniforms.addToSet(m_frameDescriptor, 1);
    m_modelUniforms.addToSet(m_modelDescriptor, 0);

    // create camera
	m_camera = ECS::createEntity();
    ECS::addComponent<ControlledCamera>(m_camera, {.aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height)});
    ECS::addComponent<NamedComponent>(m_camera, {"Camera"});

	VulkanEngine::addUpdateListener(m_modelSelector.get());
}

void Renderer3D::render(const vk::raii::CommandBuffer &commandBuffer, const vk::Image& image, const vk::ImageView& imageView) {
	beginRender(commandBuffer, image, imageView);
	setDynamicParameters(commandBuffer);
	drawSkybox(commandBuffer);
	setFrameUniforms(commandBuffer);
	drawModels(commandBuffer);
	m_boundingVolumeRenderer->draw(commandBuffer);
	VulkanEngine::getDebugWindow()->draw(commandBuffer);
	endRender(commandBuffer, image);
}

void Renderer3D::onEntityAdd(const ECS::Entity entity) {
	System::onEntityAdd(entity);
	const auto& modelInfo = ECS::getComponent<Model3D>(entity);
	if (!m_sortedEntities.contains(modelInfo.material)) {
		m_sortedEntities.insert({modelInfo.material, std::vector<ECS::Entity>()});
	}
	m_sortedEntities.at(modelInfo.material).push_back(entity);
}

void Renderer3D::onEntityRemove(const ECS::Entity entity) {
	System::onEntityRemove(entity);
	const auto& modelInfo = ECS::getComponent<Model3D>(entity);
	std::erase(m_sortedEntities.at(modelInfo.material), entity);
}

const Pipeline * Renderer3D::getPipeline() const {
	return m_pipeline.get();
}

void Renderer3D::rebuild() {
	createAttachments();
	createPipelines();
	VulkanEngine::getDebugWindow()->rebuild();
	m_boundingVolumeRenderer->rebuild();
}

void Renderer3D::setExtent(vk::Extent2D extent) {
	m_extent = extent;
	ECS::getComponent<ControlledCamera>(m_camera).aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
	createAttachments();
	m_modelSelector->setExtent(extent);
}

RendererDebugInfo Renderer3D::getDebugInfo() const {
	return m_debugInfo;
}

BoundingVolumeRenderer * Renderer3D::getBoundingVolumeRenderer() const {
	return m_boundingVolumeRenderer.get();
}

ModelSelector * Renderer3D::getModelSelector() const {
	return m_modelSelector.get();
}

const std::vector<ECS::Entity> & Renderer3D::getLastRenderedEntities() const {
	return m_renderedEntities;
}

ECS::Entity Renderer3D::getCamera() const {
	return m_camera;
}

void Renderer3D::setSampleCount(const vk::SampleCountFlagBits samples) {
	m_samples = samples;
	VulkanEngine::queueRendererRebuild();
}

vk::SampleCountFlagBits Renderer3D::getSampleCount() const {
	return m_samples;
}

void Renderer3D::setSkybox(const std::shared_ptr<Skybox> &skybox) {
	m_skybox = skybox;
	m_skybox->addToSet(m_skyboxDescriptor, 1);
}

void Renderer3D::createPipelines() {
	m_pipeline = Pipeline::Builder()
		.addShaderStage("shaders/model.vert.spv")
        .addShaderStage("shaders/model.frag.spv")
        .setVertexInfo(Vertex::getBindingDescription(), Vertex::getAttributeDescriptions())
		.addAttachment(VulkanEngine::getSwapColourFormat())
		.setSamples(m_samples)
		.enableAlphaBlending()

        .addBinding(0, 0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex) // view / project
        .addBinding(0, 1, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eFragment) // frame data - lights & camera

        .addBinding(1, 0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment) // material - base
        .addBinding(1, 1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment) // material - mr
        .addBinding(1, 2, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment) // material - ao
        .addBinding(1, 3, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment) // material - normal

        .addBinding(2, 0, vk::DescriptorType::eUniformBufferDynamic, vk::ShaderStageFlagBits::eVertex) // model data
		.create();

	m_xrayPipeline = Pipeline::Builder()
		.addShaderStage("shaders/xray.vert.spv")
		.addShaderStage("shaders/xray.frag.spv")
		.setVertexInfo(Vertex::getBindingDescription(), Vertex::getAttributeDescriptions())
		.addAttachment(VulkanEngine::getSwapColourFormat())
		.setPolygonMode(vk::PolygonMode::eLine)
		.setSamples(m_samples)
		.disableDepthTest()
		.addBinding(0, 0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex)
		.addBinding(2, 0, vk::DescriptorType::eUniformBufferDynamic, vk::ShaderStageFlagBits::eVertex)
        .addBinding(0, 1, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eFragment) // unused
		.create();

	m_skyboxPipeline = Pipeline::Builder()
		.addShaderStage("shaders/skybox.vert.spv")
		.addShaderStage("shaders/skybox.frag.spv")
		.setVertexInfo(Vertex::getBindingDescription(), Vertex::getAttributeDescriptions())
		.addAttachment(VulkanEngine::getSwapColourFormat())
		.setSamples(m_samples)
		.disableDepthTest()
		.addBinding(0, 0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex)
		.addBinding(0, 1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment)
		.create();
}

void Renderer3D::createAttachments() {
	m_depthImage = std::make_unique<Image>(ImageCreateInfo{
		.width = m_extent.width,
		.height = m_extent.height,
		.format = VulkanEngine::getDepthFormat(),
		.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
		.aspect = vk::ImageAspectFlagBits::eDepth,
		.samples = m_samples,
	});

	m_colorImage = std::make_unique<Image>(ImageCreateInfo{
		.width = m_extent.width,
		.height = m_extent.height,
		.format = VulkanEngine::getSwapColourFormat(),
		.usage = vk::ImageUsageFlagBits::eColorAttachment,
		.samples = m_samples
	});
}

void Renderer3D::beginRender(const vk::raii::CommandBuffer& commandBuffer, const vk::Image& image, const vk::ImageView& imageView) const {
	Image::changeLayout(commandBuffer, image, {vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal});
	vk::RenderingAttachmentInfo colourAttachment = {
		.imageView = imageView,
		.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.clearValue = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 0.0f),
	};
	if (m_samples != vk::SampleCountFlagBits::e1) {
		colourAttachment.imageView = m_colorImage->getView();
		colourAttachment.resolveMode = vk::ResolveModeFlagBits::eAverage;
		colourAttachment.resolveImageView = imageView;
		colourAttachment.resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal;
	}

	const std::array attachments = { colourAttachment };

	vk::RenderingAttachmentInfo depthAttachment = {
		.imageView = m_depthImage->getView(),
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
		.colorAttachmentCount = static_cast<u32>(attachments.size()),
		.pColorAttachments = attachments.data(),
		.pDepthAttachment = &depthAttachment
	};

	commandBuffer.beginRendering(renderingInfo);
}

void Renderer3D::setDynamicParameters(const vk::raii::CommandBuffer& commandBuffer) const {
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
}

void Renderer3D::setFrameUniforms(const vk::raii::CommandBuffer& commandBuffer) {
	const auto camera = ECS::getSystem<ControlledCameraSystem>();
	const auto& cameraData = ECS::getComponent<ControlledCamera>(m_camera);
	m_frameUniforms.setData({
		.view = camera->getViewMatrix(),
		.projection = camera->getProjectionMatrix(),
	});

	u32 nLights;
	m_fragFrameUniforms.setData({
		.cameraPosition = cameraData.position,
		.lights = ECS::getSystem<LightSystem>()->getLights(nLights),
		.nLights = nLights,
		.far = cameraData.far,
		.fog = cameraData.far / 20.0f
	});
	commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline->getLayout(), FRAME_SET_NUMBER, {*m_frameDescriptor}, nullptr);
}

void Renderer3D::drawModels(const vk::raii::CommandBuffer& commandBuffer) {
	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline->getPipeline());
	m_debugInfo = {
		.totalInstanceCount = 0,
		.renderedInstanceCount = 0,
		.materialSwitches = 0
	};

	const Frustum cameraFrustum = ECS::getSystem<ControlledCameraSystem>()->getFrustum();

	m_renderedEntities.clear();
	i32 highlightedIndex = ECS::NULL_ENTITY;
	i32 i = 0;
	for (const auto& [material, entities] : m_sortedEntities) {
		bool firstRendered = true;
		for (const ECS::Entity entity : entities) {
			++m_debugInfo.totalInstanceCount;

			if (!cameraFrustum.intersects(ECS::getComponent<BoundingVolume>(entity).obb)) continue;

			m_renderedEntities.push_back(entity);

			++m_debugInfo.renderedInstanceCount;

			auto& modelInfo = ECS::getComponent<Model3D>(entity);
			auto& modelTransform = ECS::getComponent<Transform>(entity);
			m_modelUniforms.setData(i, {modelTransform.transform, glm::mat4(glm::mat3(glm::inverseTranspose(modelTransform.transform)))});

			commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline->getLayout(), MODEL_SET_NUMBER, {*m_modelDescriptor}, {i * m_modelUniforms.getItemSize()});

			if (firstRendered) {
				material->use(commandBuffer, m_pipeline->getLayout());
				++m_debugInfo.materialSwitches;
				firstRendered = false;
			}

			modelInfo.mesh->draw(commandBuffer);

			if (m_highlightedEntity == entity) highlightedIndex = i;
			++i;
		}
	}
	if (highlightedIndex != ECS::NULL_ENTITY) {
		commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_xrayPipeline->getLayout(), FRAME_SET_NUMBER, {*m_frameDescriptor}, nullptr);
		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_xrayPipeline->getPipeline());
		commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_xrayPipeline->getLayout(), MODEL_SET_NUMBER, {*m_modelDescriptor}, {highlightedIndex * m_modelUniforms.getItemSize()});
		ECS::getComponent<Model3D>(m_highlightedEntity).mesh->draw(commandBuffer);
	}
}

void Renderer3D::drawSkybox(const vk::raii::CommandBuffer &commandBuffer) {
	if (m_skybox == nullptr) return;

	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_skyboxPipeline->getPipeline());
	commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_skyboxPipeline->getLayout(), FRAME_SET_NUMBER, {m_skyboxDescriptor}, nullptr);
	VulkanEngine::getAssetManager()->getUnitCube()->draw(commandBuffer);
}

void Renderer3D::endRender(const vk::raii::CommandBuffer& commandBuffer, const vk::Image& image) const {
	commandBuffer.endRendering();
	Image::changeLayout(commandBuffer, image, {vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR});
}

void Renderer3D::highlightEntity(ECS::Entity entity) {
	m_highlightedEntity = entity;
}

ECS::Entity Renderer3D::getHighlightedEntity() const {
	return m_highlightedEntity;
}
