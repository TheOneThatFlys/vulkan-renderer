#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Plane represented by the r.n = d
struct Plane {
    glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);
    float d = 0.0f;

    Plane() = default;
    // construct a plane from a normal and distance from origin
    Plane(const glm::vec3& normal, const float d) : normal(normal), d(d) {}
    // construct a plane from a normal and a point on the plane
    Plane(const glm::vec3& normal, const glm::vec3& point) : normal(normal), d(glm::dot(point, normal)) {};

    float distanceToSigned(const glm::vec3& point) const {
        return glm::dot(normal, point) - d;
    }
};

struct Sphere {
    glm::vec3 center;
    float radius;

    // returns true if the sphere intersects or is forward from the plane (based on plane normal)
    bool intersectsOrForwards(const Plane& plane) const {
        return plane.distanceToSigned(center) > -radius;
    }
};

struct OBB {
    glm::vec3 center;
    glm::vec3 extent;
    glm::quat rotation;

    // returns true if the OBB intersects or is forward from the plane (based on plane normal)
    bool intersectsOrForwards(const Plane& plane) const {
        const glm::quat invRotation = glm::conjugate(rotation);
        const glm::vec3 rotatedNormal = glm::normalize(invRotation * plane.normal);
        const glm::vec3 pointOnPlane = plane.normal * plane.d - center;
        const Plane rotatedPlane = { rotatedNormal, glm::dot(rotatedNormal, invRotation * pointOnPlane + center) };
        // adapted from https://gdbooks.gitbooks.io/3dcollisions/content/Chapter2/static_aabb_plane.html
        const float r = glm::dot(extent, glm::abs(rotatedNormal));
        return -r <= rotatedPlane.distanceToSigned(center);
    }
};

struct Frustum {
    Plane top, bottom, right, left, near, far;

    bool intersects(const Sphere& sphere) const {
        return sphere.intersectsOrForwards(top)
            && sphere.intersectsOrForwards(bottom)
            && sphere.intersectsOrForwards(right)
            && sphere.intersectsOrForwards(left)
            && sphere.intersectsOrForwards(near)
            && sphere.intersectsOrForwards(far);
    }

    bool intersects(const OBB& obb) const {
        return obb.intersectsOrForwards(top)
            && obb.intersectsOrForwards(bottom)
            && obb.intersectsOrForwards(right)
            && obb.intersectsOrForwards(left)
            && obb.intersectsOrForwards(near)
            && obb.intersectsOrForwards(far);
    }
};
