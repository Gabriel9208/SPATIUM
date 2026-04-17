# SDD: Camera System — Camera, CameraController, ProjectionMode

> **Version**: 1.0
> **Status**: `READY`
> **Created**: 2026-04-17
> **Last Updated**: 2026-04-17
> **Author**: Gabriel
> **Stage**: 3 of 5

---

## 0. Quick Reference (for Claude Code)

```
TASK   : Implement a quaternion-based free-flying camera + a controller
         that consumes input events from SDD_01 and translates them into
         camera mutations.
SCOPE  : core/camera/ProjectionMode.h
         core/camera/Camera.{h,cpp}
         core/camera/CameraController.{h,cpp}
GOAL   : Camera produces correct view/projection matrices, no gimbal lock,
         supports perspective/orthographic switching. Controller mutates
         a bound Camera on key/mouse/scroll events.
AVOID  : Storing Euler angles. Storing eye/center/up as state. Making
         CameraController self-subscribe to EventBus. Recomputing matrices
         on every mutation (use dirty flags).
STATUS : Ready for implementation. No open questions.
```

---

## 1. Context & Background

### 1.1 Why This Task Exists

The engine needs a view: a way to position the "eye", orient it, and
produce the `glm::mat4` view/projection matrices the renderer will
upload as uniforms (SDD_04). This SDD implements the Camera as a
quaternion-oriented free-flying viewpoint, plus a CameraController that
translates input events into Camera mutations.

The Camera's public API intentionally exposes familiar `move_*` / `turn_*`
methods so consumers don't have to think in quaternions. Quaternions are
an implementation detail — they exist to prevent gimbal lock and
numerical drift, not to impose a mental model on the user.

- **Problem being solved**: No view / no input-driven navigation.
- **Relation to other modules**: Consumes input events from SDD_01 via
  `EventBus` (wired in SDD_05). Consumed by Renderer (SDD_04) via
  `get_view()` / `get_projection()`.
- **Dependencies (upstream)**: GLM (existing), `events.h` extensions
  (SDD_01). SDD_02 is not a dependency.
- **Dependents (downstream)**: SDD_04 reads matrices; SDD_05 composes
  Camera and CameraController into App and wires EventBus subscriptions.

### 1.2 Reference Materials

| Resource | Path / URL | Notes |
|----------|-----------|-------|
| CONSTRAINTS.md | `./CONSTRAINTS.md` | **GD-04** (quaternion orientation), **GD-02** (controller doesn't self-subscribe) |
| SDD_01 | `./SDD_01_InputEvents.md` | Event types consumed: `KeyEvent`, `MouseMoveEvent`, `MouseClickEvent`, `ScrollEvent` |
| GLM quaternion docs | https://glm.g-truc.net/0.9.9/api/a00308.html | `glm::quat`, `angleAxis`, `quatLookAt`, `conjugate`, `mat4_cast` |
| glm::lookAt reference | https://glm.g-truc.net/0.9.9/api/a00247.html | For verifying T-05 matrix equivalence |

---

## 2. Specification

### 2.1 Objective

**Definition of Done**:
- [ ] `Camera` internal state is `position_ + orientation_ (glm::quat) + focus_distance_ + fov/aspect/near/far + ProjectionMode`.
  No stored `center_` / `up_`.
- [ ] Yaw rotates around world up; pitch rotates around local right.
- [ ] Orientation quaternion is renormalized after every rotation mutation.
- [ ] `get_view()` and `get_projection()` return `const glm::mat4&` into
  a lazy cache (dirty flags).
- [ ] `set_projection_mode` switches perspective ↔ orthographic,
  preserving framing at `focus_distance_`.
- [ ] `CameraController` does not self-subscribe to EventBus; it exposes
  `on_*` handlers that `App` will call.
- [ ] No gimbal lock: pitch 89° → yaw 90° → pitch -89° yields no roll.

### 2.2 Input / Output Contract

**`Camera::get_view()` / `get_projection()`**:
- **Inputs**: None (accessor).
- **Outputs**: `const glm::mat4&` — reference into the internal cache.
- **Side effects**: May recompute the cache if dirty.
- **Safety**: Reference valid until the next Camera mutation. Do NOT
  store the reference past any Camera mutator call.

**`CameraController::on_key` / `on_mouse_move` / `on_click` / `on_scroll`**:
- **Inputs**: Corresponding `const EventT&`.
- **Outputs**: None.
- **Side effects**: Mutates the bound `Camera*` (if set).

### 2.3 Functional Requirements

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-01 | `ProjectionMode` is `enum class { kPerspective, kOrthogonal }`. | MUST |
| FR-02 | `Camera` stores `position_ (vec3)`, `orientation_ (quat)`, `focus_distance_ (float)`, `fov_deg_`, `aspect_`, `near_`, `far_`, `mode_`. No Euler angles, no `center_` / `up_`. | MUST |
| FR-03 | Constructor takes `(eye, center, up, fov_deg, aspect, near, far)` and converts to internal quaternion state: `position_ = eye`, `focus_distance_ = length(center-eye)`, `orientation_ = glm::quatLookAt(normalize(center-eye), up)`. | MUST |
| FR-04 | `turn_left(a)` / `turn_right(a)` rotate around `kWorldUp = {0,1,0}`. `turn_up(a)` / `turn_down(a)` rotate around `right()`. All parameters in **radians**. | MUST |
| FR-05 | After every rotation mutator, `orientation_` is renormalized via `glm::normalize`. | MUST |
| FR-06 | `move_left/right/up/down/forward/backward(a)` translate `position_` along the camera-local axis (derived from `orientation_`). | MUST |
| FR-07 | `get_view()` returns `const glm::mat4&` into a `mutable` cache; recomputes when `view_dirty_` is true. | MUST |
| FR-08 | `get_projection()` returns `const glm::mat4&` into a `mutable` cache; recomputes when `projection_dirty_` is true. | MUST |
| FR-09 | View matrix is derived as `mat4_cast(conjugate(orientation_)) * translate(-position_)`. No use of `glm::lookAt` at runtime after construction. | MUST |
| FR-10 | `set_projection_mode` calls `pers_to_orth_` or `orth_to_pers_` to rebuild the projection matrix preserving framing at `focus_distance_`. | MUST |
| FR-11 | `set_aspect` marks projection dirty only (not view). | MUST |
| FR-12 | `CameraController` takes `Camera&` via `bind_camera` or constructor; stores `Camera*`. All `on_*` handlers are safe when `camera_ == nullptr` (early return). | MUST |
| FR-13 | `CameraController` does not `#include "core/event/EventBus.h"`. Wiring is `App`'s job (SDD_05). | MUST |
| FR-14 | `CameraController::on_key` reacts to `Key::kW/kA/kS/kD` with translation; `Key::kUp/kDown/kLeft/kRight` with rotation (optional binding, see §3.3.4). | MUST |
| FR-15 | `CameraController::on_mouse_move` rotates the camera only while `rotating_` flag is set (toggled by right-click press/release). | MUST |
| FR-16 | `CameraController::on_scroll` zooms via `move_forward(scroll.dy * zoom_speed_)`. | MUST |

### 2.4 Non-Functional Requirements

| Category | Requirement |
|----------|-------------|
| Performance | Matrix recompute only when cache is dirty. Quaternion normalization cost is ~6 mul + 1 sqrt per rotation — negligible. |
| Memory | Fixed-size; ~128 bytes per Camera. |
| Compatibility | C++17, GLM 1.0.3. |
| Code Style | See CONSTRAINTS.md §3. |

---

## 3. Design

### 3.1 High-Level Approach

Quaternion-oriented Camera with lazy matrix cache. Rotation mutators
apply a delta quaternion and renormalize; translation mutators adjust
`position_` along a live camera-local axis derived from the quaternion.

CameraController is a thin translation layer: each `on_*` method converts
input-domain units (pixels, scroll ticks, key presses) into Camera-domain
units (radians, world units) and calls the corresponding Camera method.

**Rejected alternatives**:
- *Euler-angle state* — rejected per GD-04 (gimbal lock).
- *glm::lookAt per frame* — rejected; requires reconstructing `center`
  from `position_ + forward()`, and `lookAt` is more expensive than the
  quaternion-direct derivation.
- *Self-subscribing controller* — rejected per GD-02.
- *Integer scroll ticks* — rejected; use `double dy` to match GLFW.

### 3.2 Architecture / Data Flow

```
            [SDD_05 App composition root]
                        │
                        ▼
    event_bus_.subscribe<KeyEvent>([&](e){ controller.on_key(e); });
    event_bus_.subscribe<MouseMoveEvent>([&](e){ controller.on_mouse_move(e); });
    event_bus_.subscribe<MouseClickEvent>([&](e){ controller.on_click(e); });
    event_bus_.subscribe<ScrollEvent>([&](e){ controller.on_scroll(e); });
                        │
                        ▼
            CameraController::on_*
                        │
                        ▼
        Camera mutators (move_*, turn_*, set_aspect)
                        │
                        ▼
       orientation_ updated, view_dirty_ = true
                        │
                        ▼
[Later, Renderer from SDD_04]  camera.get_view() / get_projection()
                        │
                        ▼
            recompute_view_ / recompute_projection_ if dirty
                        │
                        ▼
                 const glm::mat4&
```

### 3.3 Key Interfaces / Implementations

#### 3.3.1 `ProjectionMode`

**File**: `core/camera/ProjectionMode.h`

```cpp
#pragma once

enum class ProjectionMode {
    kPerspective,
    kOrthogonal,
};
```

#### 3.3.2 `Camera`

**File**: `core/camera/Camera.h` / `Camera.cpp`

```cpp
#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "ProjectionMode.h"

class Camera {
public:
    Camera(glm::vec3 eye,
           glm::vec3 center,
           glm::vec3 up,
           float fov_deg,
           float aspect,
           float near_plane,
           float far_plane);

    // Translation (camera-local axes, derived from orientation_).
    void move_left    (float amount);
    void move_right   (float amount);
    void move_up      (float amount);
    void move_down    (float amount);
    void move_forward (float amount);
    void move_backward(float amount);

    // Rotation (RADIANS). Yaw → world up; pitch → local right.
    void turn_left (float angle_rad);
    void turn_right(float angle_rad);
    void turn_up   (float angle_rad);
    void turn_down (float angle_rad);

    void set_aspect(float aspect);

    // Matrix accessors — reference into internal cache.
    // Do NOT hold across a Camera mutation.
    const glm::mat4& get_view()       const;
    const glm::mat4& get_projection() const;

    void set_projection_mode(ProjectionMode mode);
    ProjectionMode get_projection_mode() const noexcept { return mode_; }

    // Derived axes (always unit length).
    glm::vec3 forward() const noexcept {
        return orientation_ * glm::vec3(0.0f, 0.0f, -1.0f);
    }
    glm::vec3 right() const noexcept {
        return orientation_ * glm::vec3(1.0f, 0.0f, 0.0f);
    }
    glm::vec3 local_up() const noexcept {
        return orientation_ * glm::vec3(0.0f, 1.0f, 0.0f);
    }

    glm::vec3 position() const noexcept { return position_; }

private:
    void recompute_view_()       const;
    void recompute_projection_() const;

    void pers_to_orth_();
    void orth_to_pers_();

    // -- Stored state --
    glm::vec3 position_;
    glm::quat orientation_;          // world→camera rotation source
    float     focus_distance_ = 1.0f;

    float fov_deg_;
    float aspect_;
    float near_;
    float far_;
    ProjectionMode mode_ = ProjectionMode::kPerspective;

    // -- Lazy caches --
    mutable glm::mat4 view_       = glm::mat4(1.0f);
    mutable glm::mat4 projection_ = glm::mat4(1.0f);
    mutable bool view_dirty_       = true;
    mutable bool projection_dirty_ = true;

    static constexpr glm::vec3 kWorldUp = glm::vec3(0.0f, 1.0f, 0.0f);
};
```

**Constructor**:

```cpp
Camera::Camera(glm::vec3 eye, glm::vec3 center, glm::vec3 up,
               float fov_deg, float aspect,
               float near_plane, float far_plane)
    : position_(eye),
      focus_distance_(glm::length(center - eye)),
      fov_deg_(fov_deg), aspect_(aspect),
      near_(near_plane), far_(far_plane) {
    glm::vec3 forward_dir = glm::normalize(center - eye);
    orientation_ = glm::quatLookAt(forward_dir, glm::normalize(up));
}
```

**Rotation mutators** (yaw-world / pitch-local):

```cpp
void Camera::turn_left(float angle_rad) {
    glm::quat delta = glm::angleAxis(angle_rad, kWorldUp);
    orientation_ = glm::normalize(delta * orientation_);
    view_dirty_ = true;
}
void Camera::turn_right(float angle_rad) { turn_left(-angle_rad); }

void Camera::turn_up(float angle_rad) {
    glm::quat delta = glm::angleAxis(angle_rad, right());
    orientation_ = glm::normalize(delta * orientation_);
    view_dirty_ = true;
}
void Camera::turn_down(float angle_rad) { turn_up(-angle_rad); }
```

**Translation mutators**:

```cpp
void Camera::move_forward (float a) { position_ += forward()  * a; view_dirty_ = true; }
void Camera::move_backward(float a) { position_ -= forward()  * a; view_dirty_ = true; }
void Camera::move_right   (float a) { position_ += right()    * a; view_dirty_ = true; }
void Camera::move_left    (float a) { position_ -= right()    * a; view_dirty_ = true; }
void Camera::move_up      (float a) { position_ += local_up() * a; view_dirty_ = true; }
void Camera::move_down    (float a) { position_ -= local_up() * a; view_dirty_ = true; }
```

**Matrix recompute**:

```cpp
void Camera::recompute_view_() const {
    const glm::mat4 rotation    = glm::mat4_cast(glm::conjugate(orientation_));
    const glm::mat4 translation = glm::translate(glm::mat4(1.0f), -position_);
    view_ = rotation * translation;
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
```

**Projection mode switching**:

```cpp
void Camera::set_projection_mode(ProjectionMode mode) {
    if (mode == mode_) return;
    mode_ = mode;
    projection_dirty_ = true;    // recompute_projection_ handles both cases
}

// pers_to_orth_ / orth_to_pers_ are retained as private helpers for the
// class diagram's shape, but in practice set_projection_mode + the
// mode branch in recompute_projection_ does all the work. Implement both
// as simple one-liners that delegate to set_projection_mode for safety:
void Camera::pers_to_orth_() { set_projection_mode(ProjectionMode::kOrthogonal); }
void Camera::orth_to_pers_() { set_projection_mode(ProjectionMode::kPerspective); }
```

**`set_aspect`**:

```cpp
void Camera::set_aspect(float aspect) {
    aspect_ = aspect;
    projection_dirty_ = true;   // view unaffected
}
```

#### 3.3.3 `CameraController`

**File**: `core/camera/CameraController.h` / `CameraController.cpp`

```cpp
#pragma once
#include "Camera.h"
#include "core/event/events.h"

class CameraController {
public:
    CameraController() = default;
    explicit CameraController(Camera& camera) : camera_(&camera) {}

    CameraController(const CameraController&) = delete;
    CameraController& operator=(const CameraController&) = delete;

    void bind_camera(Camera& cam) noexcept { camera_ = &cam; }

    // Event handlers. Safe to call when no camera is bound.
    void on_key       (const KeyEvent&         e);
    void on_mouse_move(const MouseMoveEvent&   e);
    void on_click     (const MouseClickEvent&  e);
    void on_scroll    (const ScrollEvent&      e);

private:
    Camera* camera_ = nullptr;

    // Tunables. Camera's rotation API is radians — these convert from
    // input-domain units to Camera-domain units.
    float translate_speed_ = 0.1f;    // world units per key-press tick
    float rotate_speed_    = 0.003f;  // radians per pixel of mouse delta
    float zoom_speed_      = 1.0f;    // world units per scroll tick
    bool  rotating_        = false;   // set while RMB is held
};
```

#### 3.3.4 Controller handler bodies

```cpp
void CameraController::on_key(const KeyEvent& e) {
    if (!camera_) return;
    if (e.action != KeyAction::kPress && e.action != KeyAction::kRepeat) return;
    const float t = translate_speed_;
    switch (e.key) {
        case Key::kW: camera_->move_forward (t); break;
        case Key::kS: camera_->move_backward(t); break;
        case Key::kA: camera_->move_left    (t); break;
        case Key::kD: camera_->move_right   (t); break;
        case Key::kUp:    camera_->turn_up   (rotate_speed_ * 10.0f); break;
        case Key::kDown:  camera_->turn_down (rotate_speed_ * 10.0f); break;
        case Key::kLeft:  camera_->turn_left (rotate_speed_ * 10.0f); break;
        case Key::kRight: camera_->turn_right(rotate_speed_ * 10.0f); break;
    }
}

void CameraController::on_mouse_move(const MouseMoveEvent& e) {
    if (!camera_) return;
    if (!rotating_) return;
    // Horizontal delta → yaw; vertical delta → pitch.
    // Sign: dragging right/down rotates the view right/down.
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
```

### 3.4 File Structure Changes

```
SPATIUM/
├── core/
│   └── camera/                      ← NEW DIRECTORY
│       ├── ProjectionMode.h         ← NEW
│       ├── Camera.h                 ← NEW
│       ├── Camera.cpp               ← NEW
│       ├── CameraController.h       ← NEW
│       └── CameraController.cpp     ← NEW
└── CMakeLists.txt                   ← MODIFY: append new .cpp files to CORE_SOURCES
```

---

## 4. Implementation Plan

### 4.1 Task Breakdown

- [ ] **Step 1**: Create `ProjectionMode.h`.
    - Completion: Includes cleanly.

- [ ] **Step 2**: Create `Camera.h` with full class declaration.

- [ ] **Step 3**: Implement constructor + `recompute_view_` + `recompute_projection_`.
    - Completion: T-01 (matrix equivalence with `glm::lookAt` / `glm::perspective`) passes.

- [ ] **Step 4**: Implement rotation mutators (`turn_*`) with normalization.
    - Completion: T-03 (quaternion unit-length invariant after 10,000 rotations) passes.

- [ ] **Step 5**: Implement translation mutators (`move_*`).
    - Completion: After `move_forward(1.0)`, `position_` changes by `forward()` × 1.

- [ ] **Step 6**: Implement `set_projection_mode` + `set_aspect`.
    - Completion: T-02 (perspective↔orthogonal round-trip identity) passes.

- [ ] **Step 7**: Write the gimbal-lock regression test (T-04).
    - Pitch 89° → yaw 90° → pitch −89°. Verify `dot(local_up(), kWorldUp) > 0.9998`.

- [ ] **Step 8**: Create `CameraController.h` + `CameraController.cpp`.
    - Implement all four handlers with nullptr guards.
    - Completion: T-07 (no crash with `camera_ == nullptr`) passes.

- [ ] **Step 9**: Update `CMakeLists.txt` — append `core/camera/Camera.cpp`
  and `core/camera/CameraController.cpp` to `CORE_SOURCES`.

- [ ] **Step 10**: Build + run all tests.
    - Command: `cmake --build build`
    - Expected: Zero warnings at `-Wall -Wextra -Wpedantic`.

### 4.2 Configuration / Constants

- `kWorldUp = {0, 1, 0}` — static constexpr member of `Camera`.
- `translate_speed_`, `rotate_speed_`, `zoom_speed_` — CameraController
  tunables, initialized to the values shown in §3.3.3.

---

## 5. Testing & Validation

### 5.1 Test Cases

| Test ID | Description | Input | Expected Output | Pass Criteria |
|---------|-------------|-------|-----------------|---------------|
| T-01 | Initial matrices | eye=(0,0,3), center=(0,0,0), up=(0,1,0), fov=45°, aspect=1, near=0.1, far=100 | `get_view()` ≡ `glm::lookAt(eye,center,up)`; `get_projection()` ≡ `glm::perspective(radians(45),1,0.1,100)` | Frobenius norm of diff < 1e-5 |
| T-02 | Projection mode round-trip | perspective → orthogonal → perspective | Final perspective matrix = initial | Element-wise equality to within 1e-6 |
| T-03 | Quaternion unit-length | 10,000 random `turn_*` calls | `\|\|orientation_\|\| - 1` < 1e-5 | Renormalization invariant holds |
| T-04 | **Gimbal-lock regression** | pitch +1.553 (89°) → yaw +1.571 (90°) → pitch −1.553 | `dot(local_up(), kWorldUp) > 0.9998` | No roll introduced |
| T-05 | `set_aspect` dirties projection only | `set_aspect(2.0)` | next `get_projection()` reflects new aspect; `get_view()` unchanged | Projection `[0][0]` updates; view identical |
| T-06 | Cache reuse | Two `get_view()` calls without mutation | Only one `recompute_view_` | Instrument with a counter and verify |
| T-07 | Controller safe without camera | `CameraController c; c.on_key(e);` | No crash | Test passes without segfault |
| T-08 | Controller mutates camera | `on_key({kW, kPress})` on bound camera | `camera.position()` changes by `translate_speed_ * forward()` | Exact vector match |
| T-09 | Right-click-drag rotation | click press, move (5,3), click release | `rotating_` toggles; during press, camera yaws by `-5 * rotate_speed_` and pitches by `-3 * rotate_speed_` | Verify `get_view()` changes during drag, not before/after |
| T-10 | Scroll zoom | `on_scroll({1.0})` | `move_forward(zoom_speed_)` called | `position()` advances by `forward()` |

### 5.2 Validation Commands

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build
./build/tests/test_camera          # matrix tests
./build/tests/test_controller      # handler tests
```

### 5.3 Acceptance Criteria

- [ ] All tests T-01 through T-10 pass.
- [ ] FR-01 through FR-16 verified.
- [ ] Clean build at `-Wall -Wextra -Wpedantic`.
- [ ] `CameraController.cpp` does NOT `#include "core/event/EventBus.h"`.
- [ ] §9 checklist walked through.

---

## 6. Constraints & Anti-Patterns

### 6.1 Hard Constraints (MUST NOT)

- ❌ Global constraints from **CONSTRAINTS.md** apply in full. Especially:
    - **GD-02** — `CameraController` MUST NOT subscribe to `EventBus` itself.
    - **GD-04** — Camera MUST use quaternion orientation.
- ❌ Do NOT store Euler angles (`yaw_`, `pitch_`, `roll_`) anywhere in `Camera`.
- ❌ Do NOT store `center_` / `up_` as state. `local_up()` is derived.
- ❌ Do NOT use `glm::lookAt` inside `recompute_view_`. Use the quaternion-direct form.
- ❌ Do NOT use local up for yaw. World up only. See GD-04 rationale.
- ❌ Do NOT forget to renormalize after rotation. The unit-length invariant is airtight-at-all-times, not eventual.
- ❌ Do NOT write bare `near` or `far` in `Camera.h` / `Camera.cpp`. Windows macro pollution. Use trailing-underscore.
- ❌ Do NOT accept degrees on the rotation API. Radians only. CameraController converts.

### 6.2 Known Pitfalls & Limitations

- ⚠️ **Quaternion drift without normalization**: 10,000 rotations without `glm::normalize` will noticeably skew the view. Always normalize after rotation mutators.
- ⚠️ **`quatLookAt` accepts direction, not target**: `glm::quatLookAt(direction, up)` takes a normalized forward direction, NOT the center point. Common mistake: passing `center` directly, which is a point, not a direction.
- ⚠️ **Up vector collinear with forward at construction**: if `up` is parallel to `center - eye`, `quatLookAt` produces NaN. Defensive: assert or handle this case.
- ⚠️ **Yaw polarity**: `glm::angleAxis(angle, axis)` rotates counterclockwise when looking down the axis from positive toward origin. For `kWorldUp = {0,1,0}`, positive angle = turn left. The controller sign choice (`-e.dx` for yaw) accounts for this.
- ⚠️ **`turn_up(π/2)` followed by any yaw**: looking straight up, `right()` becomes ambiguous. Quaternion stays well-defined, but further pitch input will effectively roll. Consider clamping pitch to ±89° in CameraController if users report issues; not in v1.
- ⚠️ **Mutable cache + const getter**: ensure `mutable` is on both `view_`, `projection_`, and the two dirty bools. Forgetting any one causes a cryptic const-correctness error.

| Limitation | Description | Severity | Possible Solution |
|---|---|---|---|
| `focus_distance_` never updates | Constant after construction; orthographic framing doesn't track the target if the camera moves. | Low | Expose `set_focus_distance` or auto-update on `move_forward/backward`. Future SDD. |
| No pitch clamp | Pitching past ±90° flips the camera. | Low | Add clamp in CameraController or Camera if needed. |
| No roll support | Only yaw + pitch. | Low | Add `roll_left/right` with `turn_around_forward` implementation. |
| Key-held vs key-pressed | Handler reacts to `kPress` + `kRepeat`; GLFW's repeat rate is OS-dependent. | Medium | Track a `std::unordered_set<Key> pressed_keys_` in CameraController and integrate over frame time. Deferred. |

---

## 7. Open Questions & Decision Log

### Open Questions

*(None — SDD ready for implementation.)*

### Decision Log

| # | Decision | Rationale | Date |
|---|----------|-----------|------|
| D1 | `Camera` uses lazy recompute with `mutable` dirty flags. | Avoids recomputing matrices on every mutation. If input produces N state changes per frame, eager recompute does N matrix derivations while lazy does 1. `mutable` is the language feature for "logically const, physically caches". | 2026-04-17 |
| D2 | `get_view` / `get_projection` return `const glm::mat4&`, not `glm::mat4`. | Avoids copying 64 bytes per getter call. Safe because the reference points into the Camera's own cache member. Caller contract: do not hold the reference across a Camera mutation. | 2026-04-17 |
| D3 | Rotation API is radians, not degrees. | Matches GLM internal conventions, no conversion overhead in the Camera. CameraController handles the unit conversion at the boundary, which is the correct place for it. | 2026-04-17 |
| D4 | `focus_distance_` stored as scalar; perspective-to-orthogonal switching uses it (not a recomputed `length(center - eye)`). | With quaternion state, `center` no longer exists as a member. Keeping a scalar is simpler than reconstructing `center = position_ + forward() * focus_distance_` every mode switch. | 2026-04-17 |
| D5 | Normalization of `orientation_` happens after every rotation mutator, not lazily inside `recompute_view_`. | Keeps the unit-length invariant airtight at all times. Lazy normalization would blur the "state vs cache" boundary that the dirty-flag pattern depends on (`orientation_` would have to become `mutable`, which is wrong: it's state, not cache). | 2026-04-17 |
| D6 | `pers_to_orth_` and `orth_to_pers_` are thin delegates to `set_projection_mode`. | The class diagram demands both methods exist, but the real work happens in the mode branch of `recompute_projection_`. Delegating keeps the class diagram's shape while avoiding code duplication. | 2026-04-17 |
| D7 | `CameraController` uses raw `Camera*`, not reference. | Reference would require construction-time binding; the diagram's default-constructor + `bind_camera` pattern requires late binding. Raw pointer with nullptr-guard is the correct representation. | 2026-04-17 |

---

## 8. Progress Tracker

### Session Log

| Session | Date | Work Completed | Remaining Issues |
|---------|------|----------------|-----------------|
| #1 | 2026-04-17 | SDD drafted, marked READY | All implementation steps pending |

### Current Blockers

*(None)*

---

## 9. Implementation Verification Checklist

### `ProjectionMode`
- [ ] `enum class` (not plain `enum`).
- [ ] Values: `kPerspective`, `kOrthogonal`.

### `Camera` — state
- [ ] `position_` (vec3), `orientation_` (quat), `focus_distance_` (float) all present.
- [ ] NO `eye_`, NO `center_`, NO `up_` as stored members.
- [ ] NO Euler-angle members (no `yaw_`, `pitch_`, `roll_`).
- [ ] `near_` and `far_` (trailing underscore) — never bare `near` / `far`.
- [ ] `mode_` initialized to `kPerspective`.
- [ ] Lazy cache: `view_`, `projection_`, `view_dirty_`, `projection_dirty_` all `mutable`.
- [ ] `kWorldUp` is `static constexpr`.

### `Camera` — behavior
- [ ] Constructor converts (eye, center, up) → (position_, orientation_) via `glm::quatLookAt`.
- [ ] Constructor stores `focus_distance_ = length(center - eye)`.
- [ ] All rotation mutators renormalize `orientation_`.
- [ ] `turn_left` / `turn_right` use `kWorldUp`, NOT `local_up()`.
- [ ] `turn_up` / `turn_down` use `right()`, NOT `kWorldUp`.
- [ ] Rotation API parameters are in radians.
- [ ] `recompute_view_` uses `mat4_cast(conjugate(orientation_)) * translate(-position_)`.
- [ ] `recompute_view_` does NOT call `glm::lookAt`.
- [ ] `recompute_projection_` branches on `mode_` and uses `focus_distance_` for ortho.
- [ ] `set_aspect` marks projection dirty only (not view).
- [ ] `get_view` / `get_projection` return `const glm::mat4&`.
- [ ] `get_view` / `get_projection` recompute when dirty.
- [ ] `forward()` / `right()` / `local_up()` / `position()` are `const noexcept`.

### `CameraController`
- [ ] Non-copyable.
- [ ] `camera_` is `Camera*` (not reference), initialized to `nullptr`.
- [ ] All four `on_*` handlers early-return when `camera_ == nullptr`.
- [ ] Does NOT `#include "core/event/EventBus.h"`.
- [ ] Does NOT self-subscribe anywhere.
- [ ] `on_key` reacts to `kPress` and `kRepeat` (not `kRelease`).
- [ ] `on_mouse_move` only acts when `rotating_ == true`.
- [ ] `on_click` toggles `rotating_` on right-click press/release only.
- [ ] `on_scroll` calls `move_forward` with `zoom_speed_` scaling.
- [ ] Tunables (`translate_speed_`, `rotate_speed_`, `zoom_speed_`) have initial values matching §3.3.3.

### Build
- [ ] Two new `.cpp` files in `CORE_SOURCES`.
- [ ] `-Wall -Wextra -Wpedantic` clean.

---

## 10. Appendix (Optional)

### 10.1 Related SDDs

- [CONSTRAINTS.md](./CONSTRAINTS.md) — GD-02, GD-04 apply.
- [SDD_01_InputEvents.md](./SDD_01_InputEvents.md) — provides the event types consumed.
- [SDD_04_Renderer.md](./SDD_04_Renderer.md) — consumes `get_view` / `get_projection` / `position`.
- [SDD_05_AppIntegration.md](./SDD_05_AppIntegration.md) — wires `EventBus::subscribe` lambdas that call `CameraController::on_*`.

### 10.2 Scratch Pad / Notes

- Consider a `set_focus_distance(float d)` accessor when arcball / orbit
  camera modes are added.
- A `KeyState` set in `CameraController` would let movement accumulate
  per frame (dt-based) instead of per keypress, giving smooth motion.
  Requires a per-frame `update(dt)` call from `App::run`. Deferred.
- Pitch clamping (`std::clamp(pitch, -89°, 89°)`) would eliminate the
  "camera flipping when looking straight up" case. Implement in
  CameraController if it becomes annoying.

---

## Outcome & Decisions

### Final Technical Choices

| Choice | Detail |
|--------|--------|
| `quatLookAt` location | In GLM 1.0.3, `glm::quatLookAt` is in `<glm/gtc/quaternion.hpp>` (non-experimental). No `GLM_ENABLE_EXPERIMENTAL` needed, and `gtx/quaternion.hpp` is not included. |
| `angleAxis` / `conjugate` location | Both pulled in transitively by `<glm/gtc/quaternion.hpp>` via `ext/quaternion_trigonometric.hpp` and `ext/quaternion_common.hpp`. Single include covers all quaternion operations. |
| `glm::perspective` / `glm::ortho` / `glm::translate` | Available via `<glm/gtc/matrix_transform.hpp>`, which is itself included by `<glm/gtc/quaternion.hpp>`. No extra include needed in `Camera.cpp` beyond `<glm/gtc/matrix_transform.hpp>` for explicitness. |
| `static constexpr glm::vec3 kWorldUp` | Compiles cleanly in GLM 1.0.3 with C++17 — `glm::vec3` constructors are `constexpr`. |
| `pers_to_orth_` / `orth_to_pers_` | Thin delegates to `set_projection_mode` as per SDD. Real work is in the `mode_` branch of `recompute_projection_`. |
| `CameraController` `camera_` as `Camera*` | Enables default construction + late `bind_camera` binding; `nullptr` guard on all handlers makes it safe before binding. |

### Rejected Alternatives

- *`glm/gtx/quaternion.hpp` (experimental)*: not needed; GTX's `quatLookAt` is superseded by the GTC version in GLM 1.0.3.
- *Storing `center_` as state*: rejected per GD-04; `forward() * focus_distance_` reconstructs the focus point on demand with no extra state.
- *`glm::lookAt` inside `recompute_view_`*: rejected per GD-04 and SDD hard constraints; the quaternion-direct form `mat4_cast(conjugate(orientation_)) * translate(-position_)` is used.

### Notes for Future SDDs

- **SDD_04 (Renderer)**: call `camera.get_view()`, `camera.get_projection()`, `camera.position()` to upload the five uniforms (`uView`, `uProjection`, `uViewPos`, `uModel`, `uNormalMatrix`).
- **SDD_05 (AppIntegration)**: wire `EventBus::subscribe<KeyEvent>`, `<MouseMoveEvent>`, `<MouseClickEvent>`, `<ScrollEvent>` to lambdas that forward to `controller_.on_*(e)`. `CameraController` has no `EventBus` member — App owns the wiring.
- `Window::set_aspect_callback` does not exist yet; `App::on_resize` should call `camera_.set_aspect(window_.get_aspect_ratio())` whenever the window resizes.

*End of SDD_03*