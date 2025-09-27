#include "ModelSelector.h"

#include <chrono>
#include <imgui.h>

#include <glm/glm.hpp>

#include "InputManager.h"
#include "Renderer3D.h"
#include "VulkanEngine.h"

ModelSelector::ModelSelector(VulkanEngine *engine, vk::Extent2D extent) : m_engine(engine), m_extent(extent), m_frameUniforms(m_engine, 0), m_modelUniforms(m_engine, 0, ECS::MAX_ENTITIES) {
    m_pipeline = Pipeline::Builder(m_engine)
        .addShaderStage("shaders/id.vert.spv")
        .addShaderStage("shaders/id.frag.spv")
        .addAttachment(getTextureFormat())
        .setVertexInfo(Vertex::getBindingDescription(), Vertex::getAttributeDescriptions())
        .addBinding(FRAME_SET_NUMBER, 0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex)
        .addBinding(MODEL_SET_NUMBER, 0, vk::DescriptorType::eUniformBufferDynamic, vk::ShaderStageFlagBits::eVertex)
        .create();

    createAttachments();

    m_modelDescriptor = m_pipeline->createDescriptorSet(MODEL_SET_NUMBER);
    m_frameDescriptor = m_pipeline->createDescriptorSet(FRAME_SET_NUMBER);

    m_frameUniforms.addToSet(m_frameDescriptor);
    m_modelUniforms.addToSet(m_modelDescriptor);
}

void ModelSelector::update(float) {
    const auto renderer = m_engine->getRenderer();
    if (m_selected != ECS::NULL_ENTITY) {
        renderer->getBoundingVolumeRenderer()->queueOBB(ECS::getComponent<BoundingVolume>(m_selected).obb, glm::vec3(1.0f, 0.657f, 0.0f));
    }

    if (!m_enabled) return;

    if (InputManager::mousePressed(GLFW_MOUSE_BUTTON_1)) {
        ECS::Entity newSelected = calculateSelectedEntity();
        if (newSelected == m_selected) {
            m_selected = ECS::NULL_ENTITY;
        }
        else {
            m_selected = newSelected;
        }
        renderer->highlightEntity(m_selected);
    }

}

void ModelSelector::enable() {
    m_enabled = true;
}

void ModelSelector::disable() {
    m_enabled = false;
}

void ModelSelector::setExtent(vk::Extent2D extent) {
    m_extent = extent;
    createAttachments();
}

ECS::Entity ModelSelector::getSelected() const {
    return m_selected;
}

ECS::Entity ModelSelector::calculateSelectedEntity() {
    glm::vec2 mousePosition = InputManager::mousePos();
    auto iMousePosition = glm::ivec2(mousePosition);
    const auto [winWidth, winHeight] = m_engine->getWindowSize();

    auto commandBuffer = m_engine->beginSingleCommand();
    m_engine->transitionImageLayout(commandBuffer, m_colourImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);

    const vk::RenderingAttachmentInfo colourAttachment = {
        .imageView = m_colourImageView,
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .clearValue = vk::ClearColorValue(*reinterpret_cast<const float*>(&ECS::NULL_ENTITY), 0.0f, 0.0f, 0.0f)
    };
    const vk::RenderingAttachmentInfo depthAttachment = {
        .imageView = m_depthImageView,
        .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eDontCare,
        .clearValue = vk::ClearDepthStencilValue(1.0f, 0)
    };

    const vk::RenderingInfo renderingInfo = {
        .renderArea = {
            .offset = {0, 0},
            .extent = {winWidth, winHeight}
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colourAttachment,
        .pDepthAttachment = &depthAttachment,
    };
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline->getPipeline());
    const vk::Viewport viewport = {
        .x = 0,
        .y = 0,
        .width = static_cast<float>(winWidth),
        .height = static_cast<float>(winHeight),
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    const vk::Rect2D scissor = {
        .offset = {0, 0},
        .extent = {winWidth, winHeight}
    };
    commandBuffer.setViewport(0, viewport);
    commandBuffer.setScissor(0, scissor);
    commandBuffer.beginRendering(renderingInfo);

    const auto camera = ECS::getSystem<ControlledCameraSystem>();
    m_frameUniforms.setData({
        .view = camera->getViewMatrix(),
        .projection = camera->getProjectionMatrix(),
    });
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline->getLayout(), FRAME_SET_NUMBER, {*m_frameDescriptor}, {});

    mousePosition /= glm::vec2(winWidth, winHeight);
    mousePosition = mousePosition * 2.0f - 1.0f;

    u32 i = 0;
    const Ray ray = camera->normalisedScreenToRay(mousePosition);
    for (const auto entity : m_engine->getRenderer()->getLastRenderedEntities()) {
        assert(ECS::hasComponent<BoundingVolume>(entity));
        assert(ECS::hasComponent<Transform>(entity));
        assert(ECS::hasComponent<Model3D>(entity));

        const auto& obb = ECS::getComponent<BoundingVolume>(entity).obb;

        if (obb.intersects(ray) >= 0.0f) {

            // valid object - render and test
            m_modelUniforms.setData(i, {ECS::getComponent<Transform>(entity).transform, entity});
            commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline->getLayout(), MODEL_SET_NUMBER, {*m_modelDescriptor}, {i * m_modelUniforms.getItemSize()});
            const auto& model3D = ECS::getComponent<Model3D>(entity);
            model3D.mesh->draw(commandBuffer);
            ++i;
        }
    }

    commandBuffer.endRendering();

    m_engine->transitionImageLayout(commandBuffer, m_colourImage, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal);
    const vk::BufferImageCopy region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .imageOffset = {iMousePosition.x, iMousePosition.y, 0},
        .imageExtent = {1, 1, 1}
    };
    commandBuffer.copyImageToBuffer(m_colourImage, vk::ImageLayout::eTransferSrcOptimal, m_outputBuffer, region);

    m_engine->endSingleCommand(commandBuffer);

    void* data = m_outputBufferMemory.mapMemory(0, m_outputBuffer.getMemoryRequirements().size);

    m_outputBufferMemory.unmapMemory();

    return *static_cast<ECS::Entity*>(data);
}

void ModelSelector::createAttachments() {
    std::tie(m_colourImage, m_colourImageMemory) = m_engine->createImage(
            m_extent.width,
            m_extent.height,
            getTextureFormat(),
            vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eDeviceLocal
        );
    m_colourImageView = m_engine->createImageView(m_colourImage, getTextureFormat(), vk::ImageAspectFlagBits::eColor);

    std::tie(m_depthImage, m_depthImageMemory) = m_engine->createImage(
        m_extent.width,
        m_extent.height,
        m_engine->getDepthFormat(),
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eDepthStencilAttachment,
        vk::MemoryPropertyFlagBits::eDeviceLocal
    );
    m_depthImageView = m_engine->createImageView(m_depthImage, m_engine->getDepthFormat(), vk::ImageAspectFlagBits::eDepth);

    std::tie(m_outputBuffer, m_outputBufferMemory) = m_engine->createBuffer(m_colourImage.getMemoryRequirements().size, vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible);
}

constexpr vk::Format ModelSelector::getTextureFormat() const {
    return vk::Format::eR32Sint;
}
