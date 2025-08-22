#include "Node.h"

#include "Logger.h"

#include <glm/gtc/matrix_transform.hpp>

Node::Node(glm::vec3 position, glm::quat rotation, glm::vec3 scale) : m_position(position), m_rotation(rotation), m_scale(scale) {
    type = eNode;
    recalculateTransform();
}

Node::~Node() {
}

glm::vec3 Node::getPosition() const {
    return m_position;
}

void Node::setPosition(const glm::vec3 &v) {
    m_position = v;
    recalculateTransform();
}

glm::quat Node::getRotation() const {
    return m_rotation;
}

void Node::setRotation(const glm::quat &v) {
    m_rotation = v;
    recalculateTransform();
}

glm::vec3 Node::getScale() const {
    return m_scale;
}

void Node::setScale(const glm::vec3 &v) {
    m_scale = v;
    recalculateTransform();
}

glm::mat4 Node::getTransform() const {
    return m_transform;
}

const std::vector<std::unique_ptr<Node>> &Node::getChildren() const {
    return m_children;
}

Node * Node::getLastChild() const {
    return m_children.back().get();
}

void Node::recalculateTransform() {
    m_transform = glm::mat4(1.0f);
    m_transform = glm::translate(m_transform, m_position);
    m_transform = glm::mat4_cast(m_rotation) * m_transform;
    m_transform = glm::scale(m_transform, m_scale);

    if (parent != nullptr) {
        m_transform = parent->getTransform() * m_transform;
    }

    for (const auto& child : m_children) {
        child->recalculateTransform();
    }
}
