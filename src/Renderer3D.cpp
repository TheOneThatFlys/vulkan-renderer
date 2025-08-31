#include "Renderer3D.h"

#include <glm/gtc/matrix_inverse.hpp>

#include "Components.h"

Renderer3D::Renderer3D(VulkanEngine *engine, const Pipeline* pipeline)
    : m_engine(engine)
    , m_pipeline(pipeline)
    , m_frameDescriptor(m_pipeline->createDescriptorSet(FRAME_SET_NUMBER))
    , m_modelDescriptor(m_pipeline->createDescriptorSet(MODEL_SET_NUMBER))
    , m_frameUniforms(m_engine, 0)
    , m_modelUniforms(m_engine, 0)
    , m_fragFrameUniforms(m_engine, 1)
{
    m_frameUniforms.addToSet(m_frameDescriptor);
    m_fragFrameUniforms.addToSet(m_frameDescriptor);
    m_modelUniforms.addToSet(m_modelDescriptor);
    // create camera
    m_camera = ECS::createEntity();
    ECS::addComponent<ControlledCamera>(m_camera, {});
    ECS::addComponent<NamedComponent>(m_camera, {"Camera"});
}

void Renderer3D::render(const vk::raii::CommandBuffer &commandBuffer) {
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

    u32 i = 1;
    for (const ECS::Entity entity : m_entities) {
        auto& modelInfo = ECS::getComponent<Model3D>(entity);
        auto& modelTransform = ECS::getComponent<Transform>(entity);
        m_modelUniforms.setData(i, {modelTransform.transform, glm::mat4(glm::mat3(glm::inverseTranspose(modelTransform.transform)))});

        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline->getLayout(), MODEL_SET_NUMBER, {*m_modelDescriptor}, {i * m_modelUniforms.getItemSize()});

        modelInfo.material->use(commandBuffer, m_pipeline->getLayout());
        modelInfo.mesh->draw(commandBuffer);
        ++i;
    }
}
