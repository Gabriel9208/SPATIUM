#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "core/camera/ProjectionMode.h"

class Camera {
public:
    Camera(glm::vec3 eye,
           glm::vec3 center,
           glm::vec3 up,
           float fov_deg,
           float aspect,
           float near_plane,
           float far_plane);

    // Translation along camera-local axes derived from orientation_.
    void move_left    (float amount);
    void move_right   (float amount);
    void move_up      (float amount);
    void move_down    (float amount);
    void move_forward (float amount);
    void move_backward(float amount);

    // Rotation (RADIANS). Yaw → world up axis; pitch → local right axis.
    void turn_left (float angle_rad);
    void turn_right(float angle_rad);
    void turn_up   (float angle_rad);
    void turn_down (float angle_rad);

    // Marks projection dirty only; view is unaffected.
    void set_aspect(float aspect);

    // Returns const ref into internal lazy cache.
    // Do NOT hold the reference across a Camera mutation.
    [[nodiscard]] const glm::mat4& get_view()       const;
    [[nodiscard]] const glm::mat4& get_projection() const;

    void           set_projection_mode(ProjectionMode mode);
    [[nodiscard]] ProjectionMode get_projection_mode() const noexcept { return mode_; }

    // Derived axes — always unit length.
    [[nodiscard]] glm::vec3 forward() const noexcept {
        return orientation_ * glm::vec3(0.0f, 0.0f, -1.0f);
    }
    [[nodiscard]] glm::vec3 right() const noexcept {
        return orientation_ * glm::vec3(1.0f, 0.0f, 0.0f);
    }
    [[nodiscard]] glm::vec3 local_up() const noexcept {
        return orientation_ * glm::vec3(0.0f, 1.0f, 0.0f);
    }

    [[nodiscard]] glm::vec3 position() const noexcept { return position_; }

private:
    void recompute_view_()       const;
    void recompute_projection_() const;

    void pers_to_orth_();
    void orth_to_pers_();

    // ---- Stored state ----
    glm::vec3 position_;
    glm::quat orientation_;
    float     focus_distance_ = 1.0f;

    float fov_deg_;
    float aspect_;
    float near_;
    float far_;
    ProjectionMode mode_ = ProjectionMode::kPerspective;

    // ---- Lazy matrix caches ----
    mutable glm::mat4 view_             = glm::mat4(1.0f);
    mutable glm::mat4 projection_       = glm::mat4(1.0f);
    mutable bool      view_dirty_       = true;
    mutable bool      projection_dirty_ = true;

    static constexpr glm::vec3 kWorldUp = glm::vec3(0.0f, 1.0f, 0.0f);
};
