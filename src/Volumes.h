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

struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;
};

struct OBB {
    glm::vec3 center;
    glm::vec3 extent;
    glm::quat rotation;

    // returns true if the OBB intersects or is forward from the plane (based on plane normal)
    bool intersectsOrForwards(const Plane& plane) const {
        // adapted from https://gdbooks.gitbooks.io/3dcollisions/content/Chapter2/static_aabb_plane.html
        const Plane rotatedPlane = transform(plane);
        const float r = glm::dot(extent, glm::abs(rotatedPlane.normal));
        return -r <= rotatedPlane.distanceToSigned(center);
    }

    // returns -1.0f if no intersections, or else the distance of the intersection
    float intersects(const Ray& ray) const {
        // adapted from https://gdbooks.gitbooks.io/3dcollisions/content/Chapter3/raycast_aabb.html
        const Ray newRay = transform(ray);
        const glm::vec3 invD = 1.0f / newRay.direction;
        const glm::vec3 min = center - extent;
        const glm::vec3 max = center + extent;

        const glm::vec3 a = (min - newRay.origin) * invD;
        const glm::vec3 b = (max - newRay.origin) * invD;

        float tmin = std::max(std::max(std::min(a.x, b.x), std::min(a.y, b.y)), std::min(a.z, b.z));
        float tmax = std::min(std::min(std::max(a.x, b.x), std::max(a.y, b.y)), std::max(a.z, b.z));

        // if tmax < 0, ray (line) is intersecting AABB, but whole AABB is behind us
        if (tmax < 0) {
            return -1.0f;
        }

        // if tmin > tmax, ray doesn't intersect AABB
        if (tmin > tmax) {
            return -1.0f;
        }

        if (tmin < 0.0f) {
            return tmax;
        }
        return tmin;
    }

    bool intersects(const glm::vec3& point) const {
        const glm::vec3 min = center - extent;
        const glm::vec3 max = center + extent;
        return point.x >= min.x && point.x <= max.x
            && point.y >= min.y && point.y <= max.y
            && point.z >= min.z && point.z <= max.z;
    }

private:
    // transform a point from world space into OBB's space
    // so that from the point's perspective the OBB is axis aligned
    glm::vec3 transform(const glm::vec3& point) const {
        const glm::quat invRotation = glm::conjugate(rotation);
        return invRotation * (point - center) + center;
    }

    Plane transform(const Plane& plane) const {
        const glm::quat invRotation = glm::conjugate(rotation);
        const glm::vec3 rotatedNormal = invRotation * plane.normal;
        const glm::vec3 pointOnPlane = plane.normal * plane.d - center;
        return Plane(
            rotatedNormal,
            glm::dot(rotatedNormal, invRotation * pointOnPlane + center)
        );
    }

    Ray transform(const Ray& ray) const {
        const glm::quat invRotation = glm::conjugate(rotation);
        return {
            .origin = invRotation * (ray.origin - center) + center,
            .direction = invRotation * ray.direction
        };
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
