#include "Renderer3D.h"

#include "Components.h"

Renderer3D::Renderer3D(VulkanEngine *engine, const Pipeline* pipeline)
    : m_engine(engine)
    , m_pipeline(pipeline)
    , m_frameDescriptor(m_pipeline->createDescriptorSet(FRAME_SET_NUMBER))
    , m_modelDescriptor(m_pipeline->createDescriptorSet(MODEL_SET_NUMBER))
    , m_frameUniforms(m_engine, 0)
    , m_modelUniforms(m_engine, 0)
{
    m_frameUniforms.addToSet(m_frameDescriptor);
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

    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline->getLayout(), FRAME_SET_NUMBER, {*m_frameDescriptor}, nullptr);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline->getLayout(), MODEL_SET_NUMBER, {*m_modelDescriptor}, nullptr);

    for (ECS::Entity entity : m_entities) {
        m_modelUniforms.setData({ECS::getComponent<Transform>(entity).transform});
        auto& modelInfo = ECS::getComponent<Model3D>(entity);
        modelInfo.material->use(commandBuffer, m_pipeline->getLayout());
        modelInfo.mesh->draw(commandBuffer);
    }
}
