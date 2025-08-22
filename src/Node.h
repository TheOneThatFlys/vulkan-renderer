#pragma once

#include <memory>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "NodeType.h"

class Node {
public:
    explicit Node(glm::vec3 position = glm::vec3(0.0f), glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3 scale = glm::vec3(1.0f));
    Node(Node&) = delete;
    ~Node();

    glm::vec3 getPosition() const;
    void setPosition(const glm::vec3& v);
    glm::quat getRotation() const;
    void setRotation(const glm::quat& v);
    glm::vec3 getScale() const;
    void setScale(const glm::vec3& v);
    glm::mat4 getTransform() const;
    const std::vector<std::unique_ptr<Node>>& getChildren() const;

    template<typename T, typename... Args>
    Node* addChild(Args&&... args) {
        m_children.emplace_back(std::make_unique<T>(args...));
        m_children.back()->parent = this;
        return m_children.back().get();
    }

    Node* getLastChild() const;
    Node* parent = nullptr;
    NodeType type;

private:
    void recalculateTransform();

    std::vector<std::unique_ptr<Node>> m_children;
    glm::vec3 m_position, m_scale;
    glm::quat m_rotation;
    glm::mat4 m_transform;
};