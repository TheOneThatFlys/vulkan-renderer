#pragma once

#include "Common.h"
#include "ECS.h"

class VulkanEngine;

class ModelSelector : public IUpdatable {
public:
    explicit ModelSelector(VulkanEngine* engine);
    ~ModelSelector() override = default;
    void update(float) override;

    void enable();
    void disable();
private:
    VulkanEngine* m_engine;

    bool m_enabled = false;
    ECS::Entity m_selected = ECS::NULL_ENTITY;
};
