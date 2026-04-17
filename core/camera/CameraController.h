#pragma once

#include "core/camera/Camera.h"
#include "core/event/events.h"

// CameraController translates input events into Camera mutations.
//
// Design (GD-02): This class does NOT subscribe itself to EventBus.
// App (SDD_05) wires the subscriptions and forwards events to on_*().
// This keeps CameraController testable without a bus: call on_key(e)
// directly and inspect camera state.
class CameraController {
public:
    CameraController() = default;
    explicit CameraController(Camera& camera) : camera_(&camera) {}

    CameraController(const CameraController&)            = delete;
    CameraController& operator=(const CameraController&) = delete;

    void bind_camera(Camera& cam) noexcept { camera_ = &cam; }

    // Event handlers. All are safe to call when no camera is bound.
    void on_key       (const KeyEvent&        e);
    void on_mouse_move(const MouseMoveEvent&  e);
    void on_click     (const MouseClickEvent& e);
    void on_scroll    (const ScrollEvent&     e);

private:
    Camera* camera_ = nullptr;

    // Input-to-Camera-domain unit conversion tunables.
    float translate_speed_ = 0.1f;   // world units per key-press tick
    float rotate_speed_    = 0.003f; // radians per pixel of mouse delta
    float zoom_speed_      = 1.0f;   // world units per scroll tick
    bool  rotating_        = false;  // true while RMB is held
};
