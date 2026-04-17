#include "core/camera/Camera.h"

#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

Camera::Camera(glm::vec3 eye,
               glm::vec3 center,
               glm::vec3 up,
               float fov_deg,
               float aspect,
               float near_plane,
               float far_plane)
    : position_(eye)
    , focus_distance_(glm::length(center - eye))
    , fov_deg_(fov_deg)
    , aspect_(aspect)
    , near_(near_plane)
    , far_(far_plane)
{
    const glm::vec3 forward_dir = glm::normalize(center - eye);
    orientation_ = glm::quatLookAt(forward_dir, glm::normalize(up));
}

// ---------------------------------------------------------------------------
// Rotation mutators — yaw-world / pitch-local, normalize after every call
// ---------------------------------------------------------------------------

void Camera::turn_left(float angle_rad) {
    const glm::quat delta = glm::angleAxis(angle_rad, kWorldUp);
    orientation_ = glm::normalize(delta * orientation_);
    view_dirty_  = true;
}

void Camera::turn_right(float angle_rad) {
    turn_left(-angle_rad);
}

void Camera::turn_up(float angle_rad) {
    const glm::quat delta = glm::angleAxis(angle_rad, right());
    orientation_ = glm::normalize(delta * orientation_);
    view_dirty_  = true;
}

void Camera::turn_down(float angle_rad) {
    turn_up(-angle_rad);
}

// ---------------------------------------------------------------------------
// Translation mutators — move along camera-local axes
// ---------------------------------------------------------------------------

void Camera::move_forward (float a) { position_ += forward()  * a; view_dirty_ = true; }
void Camera::move_backward(float a) { position_ -= forward()  * a; view_dirty_ = true; }
void Camera::move_right   (float a) { position_ += right()    * a; view_dirty_ = true; }
void Camera::move_left    (float a) { position_ -= right()    * a; view_dirty_ = true; }
void Camera::move_up      (float a) { position_ += local_up() * a; view_dirty_ = true; }
void Camera::move_down    (float a) { position_ -= local_up() * a; view_dirty_ = true; }

// ---------------------------------------------------------------------------
// Matrix recompute — runs only when dirty flag is set
// ---------------------------------------------------------------------------

void Camera::recompute_view_() const {
    const glm::mat4 rotation    = glm::mat4_cast(glm::conjugate(orientation_));
    const glm::mat4 translation = glm::translate(glm::mat4(1.0f), -position_);
    view_       = rotation * translation;
    view_dirty_ = false;
}

void Camera::recompute_projection_() const {
    if (mode_ == ProjectionMode::kPerspective) {
        projection_ = glm::perspective(glm::radians(fov_deg_),
                                       aspect_, near_, far_);
    } else {
        const float half_h = focus_distance_ *
                             std::tan(glm::radians(fov_deg_) * 0.5f);
        const float half_w = half_h * aspect_;
        projection_ = glm::ortho(-half_w, half_w, -half_h, half_h,
                                 near_, far_);
    }
    projection_dirty_ = false;
}

const glm::mat4& Camera::get_view() const {
    if (view_dirty_) recompute_view_();
    return view_;
}

const glm::mat4& Camera::get_projection() const {
    if (projection_dirty_) recompute_projection_();
    return projection_;
}

// ---------------------------------------------------------------------------
// Projection mode switching
// ---------------------------------------------------------------------------

void Camera::set_projection_mode(ProjectionMode mode) {
    if (mode == mode_) return;
    mode_             = mode;
    projection_dirty_ = true;
}

void Camera::pers_to_orth_() { set_projection_mode(ProjectionMode::kOrthogonal);  }
void Camera::orth_to_pers_() { set_projection_mode(ProjectionMode::kPerspective); }

void Camera::set_aspect(float aspect) {
    aspect_           = aspect;
    projection_dirty_ = true;  // view is unaffected
}
