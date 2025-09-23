#include "ModelSelector.h"

#include "InputManager.h"
#include "Renderer3D.h"
#include "VulkanEngine.h"

ModelSelector::ModelSelector(VulkanEngine *engine) : m_engine(engine) {

}

void ModelSelector::update(float) {
    if (!m_enabled) return;

    const auto renderer = m_engine->getRenderer();
    if (InputManager::mousePressed(GLFW_MOUSE_BUTTON_1)) {
        m_selected = ECS::NULL_ENTITY;

        const auto [winWidth, winHeight] = m_engine->getWindowSize();
        glm::vec2 mousePosition = InputManager::mousePos();
        mousePosition /= glm::vec2(winWidth, winHeight);
        mousePosition = mousePosition * 2.0f - 1.0f;

        const Ray ray = ECS::getSystem<ControlledCameraSystem>()->normalisedScreenToRay(mousePosition);
        float minDistance = FLT_MAX;
        for (const auto entity : renderer->m_entities) {
            // if (!ECS::hasComponent<BoundingVolume>(entity)) Logger::warn("Using invalid OBB");
            const auto& obb = ECS::getComponent<BoundingVolume>(entity).obb;

            // ignore if inside the bounding box
            if (obb.intersects(ray.origin)) continue;

            float distance = obb.intersects(ray);
            if (distance >= 0 && distance < minDistance) {
                m_selected = entity;
                minDistance = distance;
            }
        }
    }

    if (m_selected != ECS::NULL_ENTITY) {
        renderer->getBoundingVolumeRenderer()->queueOBB(ECS::getComponent<BoundingVolume>(m_selected).obb, glm::vec3(1.0f, 0.0f, 1.0f));
        renderer->highlightEntity(m_selected);
    }
}

void ModelSelector::enable() {
    m_enabled = true;
}

void ModelSelector::disable() {
    m_enabled = false;
    if (m_selected != ECS::NULL_ENTITY) {
        m_engine->getRenderer()->highlightEntity(ECS::NULL_ENTITY);
    }
}
