/* *
 * kern/engine/scene_graph/transform.h - Transform Component
 * 
 * Week 2 Production Implementation
 * 
 * Features:
 * - Local space (position, rotation, scale)
 * - World space (cached matrix, dirty flag)
 * - Hierarchy integration (parent/children)
 * - Transform propagation (parent affects children)
 */

#pragma once

#include "../core/component.h"
#include "../kern_engine_contracts.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <vector>

// ═══════════════════════════════════════════════════════════════════════════════
// TRANSFORM COMPONENT
// ═══════════════════════════════════════════════════════════════════════════════

namespace kern::engine {

// Forward declaration - World manages the hierarchy
class World;

struct Transform : public ComponentBase {
    // ═══════════════════════════════════════════════════════════════════════════
    // DATA CONTRACT COMPLIANCE
    // ═══════════════════════════════════════════════════════════════════════════
    
    static_assert(sizeof(glm::vec3) == 12, "glm::vec3 must be 12 bytes");
    static_assert(sizeof(glm::quat) == 16, "glm::quat must be 16 bytes");
    static_assert(sizeof(glm::mat4) == 64, "glm::mat4 must be 64 bytes");
    
    // Total: 12 + 16 + 12 + 64 + 1 + 3 + 4 = 112 bytes
    // Under 128-byte threshold for cache efficiency
    
    KERN_CONTRACT_COMPONENT_TYPE(Transform);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // LOCAL SPACE (Editable by user/systems)
    // ═══════════════════════════════════════════════════════════════════════════
    
    // Position in parent's space
    glm::vec3 localPosition = glm::vec3(0.0f, 0.0f, 0.0f);
    
    // Rotation in parent's space (quaternion)
    glm::quat localRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // Identity
    
    // Scale in parent's space
    glm::vec3 localScale = glm::vec3(1.0f, 1.0f, 1.0f);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // WORLD SPACE (Computed, cached, read-only from user's perspective)
    // ═══════════════════════════════════════════════════════════════════════════
    
    // World transformation matrix (cached)
    glm::mat4 worldMatrix = glm::mat4(1.0f);
    
    // Flag: worldMatrix needs recalculation
    // Set to true when local transform or parent changes
    bool worldDirty = true;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // HIERARCHY (Managed by World, not user-editable directly)
    // ═══════════════════════════════════════════════════════════════════════════
    
    // Parent entity (INVALID_ENTITY = no parent = root)
    EntityId parent = INVALID_ENTITY;
    
    // Children entities (managed by World)
    // Note: This is stored here for fast iteration, but managed by World
    std::vector<EntityId> children;  // TODO: Consider SmallVector optimization
    
    // ═══════════════════════════════════════════════════════════════════════════
    // LOCAL SPACE ACCESSORS
    // ═══════════════════════════════════════════════════════════════════════════
    
    void setLocalPosition(const glm::vec3& pos) {
        localPosition = pos;
        markDirty();
    }
    
    void setLocalRotation(const glm::quat& rot) {
        localRotation = rot;
        markDirty();
    }
    
    void setLocalEulerAngles(const glm::vec3& angles) {
        // angles in radians (pitch, yaw, roll)
        localRotation = glm::quat(angles);
        markDirty();
    }
    
    void setLocalScale(const glm::vec3& scale) {
        localScale = scale;
        markDirty();
    }
    
    // ═══════════════════════════════════════════════════════════════════════════
    // WORLD SPACE READ-ONLY ACCESSORS (calculated on demand)
    // ═══════════════════════════════════════════════════════════════════════════
    
    // Get world position (from worldMatrix)
    glm::vec3 getWorldPosition() const {
        return glm::vec3(worldMatrix[3]);
    }
    
    // Get world rotation (extracted from worldMatrix)
    glm::quat getWorldRotation() const;
    
    // Get world scale (extracted from worldMatrix)
    glm::vec3 getWorldScale() const;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // DIRECTION VECTORS (in world space)
    // ═══════════════════════════════════════════════════════════════════════════
    
    glm::vec3 getForward() const {
        // Forward is +Z in local space, transformed to world
        return glm::normalize(glm::vec3(worldMatrix[2]));
    }
    
    glm::vec3 getRight() const {
        // Right is +X in local space
        return glm::normalize(glm::vec3(worldMatrix[0]));
    }
    
    glm::vec3 getUp() const {
        // Up is +Y in local space
        return glm::normalize(glm::vec3(worldMatrix[1]));
    }
    
    // ═══════════════════════════════════════════════════════════════════════════
    // WORLD SPACE SETTERS (requires parent transform knowledge - delegated to World)
    // ═══════════════════════════════════════════════════════════════════════════
    
    // These are implemented in world.cpp where we have access to hierarchy
    void setWorldPosition(const glm::vec3& pos, const glm::mat4& parentWorldMatrix);
    void setWorldRotation(const glm::quat& rot, const glm::mat4& parentWorldMatrix);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // HIERARCHY MARKING
    // ═══════════════════════════════════════════════════════════════════════════
    
    void markDirty() {
        worldDirty = true;
    }
    
    bool isDirty() const {
        return worldDirty;
    }
    
    // ═══════════════════════════════════════════════════════════════════════════
    // MATRIX CALCULATION (internal use by World)
    // ═══════════════════════════════════════════════════════════════════════════
    
    // Calculate local matrix from local position/rotation/scale
    glm::mat4 calculateLocalMatrix() const {
        glm::mat4 T = glm::translate(glm::mat4(1.0f), localPosition);
        glm::mat4 R = glm::mat4_cast(localRotation);
        glm::mat4 S = glm::scale(glm::mat4(1.0f), localScale);
        return T * R * S;  // TRS order
    }
    
    // Recalculate world matrix given parent's world matrix
    void updateWorldMatrix(const glm::mat4& parentWorldMatrix) {
        worldMatrix = parentWorldMatrix * calculateLocalMatrix();
        worldDirty = false;
    }
    
    // For root entities (no parent)
    void updateWorldMatrixAsRoot() {
        worldMatrix = calculateLocalMatrix();
        worldDirty = false;
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// UTILITY FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════════

// Convert Euler angles (degrees) to quaternion
inline glm::quat eulerDegreesToQuat(float pitch, float yaw, float roll) {
    return glm::quat(glm::vec3(
        glm::radians(pitch),
        glm::radians(yaw),
        glm::radians(roll)
    ));
}

// Look at function (create rotation that looks at target)
inline glm::quat lookAt(const glm::vec3& forward, const glm::vec3& up = glm::vec3(0, 1, 0)) {
    glm::vec3 f = glm::normalize(forward);
    glm::vec3 r = glm::normalize(glm::cross(up, f));
    glm::vec3 u = glm::cross(f, r);
    
    glm::mat3 m(r, u, f);
    return glm::quat_cast(m);
}

} // namespace kern::engine
