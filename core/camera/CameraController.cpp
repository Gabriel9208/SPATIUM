#include "core/camera/CameraController.h"

void CameraController::on_key(const KeyEvent& e) {
    if (!camera_) return;
    if (e.action != KeyAction::kPress && e.action != KeyAction::kRepeat) return;

    const float t = translate_speed_;
    switch (e.key) {
        case Key::kW: camera_->move_forward (t); break;
        case Key::kS: camera_->move_backward(t); break;
        case Key::kA: camera_->move_left    (t); break;
        case Key::kD: camera_->move_right   (t); break;
        // Arrow keys for rotation (scaled to give a reasonable per-tick step).
        case Key::kUp:    camera_->turn_up   (rotate_speed_ * 10.0f); break;
        case Key::kDown:  camera_->turn_down (rotate_speed_ * 10.0f); break;
        case Key::kLeft:  camera_->turn_left (rotate_speed_ * 10.0f); break;
        case Key::kRight: camera_->turn_right(rotate_speed_ * 10.0f); break;
    }
}

void CameraController::on_mouse_move(const MouseMoveEvent& e) {
    if (!camera_) return;
    if (!rotating_) return;

    // Dragging right → yaw right; dragging down → pitch down.
    // Negative sign because mouse X increases right but turn_left is positive-left.
    const float yaw   = static_cast<float>(-e.dx) * rotate_speed_;
    const float pitch = static_cast<float>(-e.dy) * rotate_speed_;
    camera_->turn_left(yaw);
    camera_->turn_up  (pitch);
}

void CameraController::on_click(const MouseClickEvent& e) {
    if (!camera_) return;
    if (e.button == Button::kRight) {
        rotating_ = (e.action == ClickAction::kPress);
    }
}

void CameraController::on_scroll(const ScrollEvent& e) {
    if (!camera_) return;
    camera_->move_forward(static_cast<float>(e.dy) * zoom_speed_);
}
