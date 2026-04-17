# SDD: Input Event Layer & Window Input Callbacks

> **Version**: 1.0
> **Status**: `READY`
> **Created**: 2026-04-17
> **Last Updated**: 2026-04-17
> **Author**: Gabriel
> **Stage**: 1 of 5

---

## 0. Quick Reference (for Claude Code)

```
TASK   : Extend core/event/events.h with input event payloads + enums.
         Extend Window with GLFW key/mouse/scroll callback bridging.
         Add Window::get_aspect_ratio().
SCOPE  : core/event/events.h   (ADDITIVE; do not alter WindowResizeEvent)
         core/window/Window.h  (ADDITIVE)
         core/window/Window.cpp (ADDITIVE)
GOAL   : Window emits KeyEvent, MouseMoveEvent, MouseClickEvent,
         ScrollEvent through user-provided callbacks; get_aspect_ratio()
         returns w/h with divide-by-zero guard.
AVOID  : Changing the existing Window constructor signature or any method
         body. Introducing EventBus into Window. Introducing any dependency
         beyond GLFW + std.
STATUS : Ready for implementation. No open questions.
```

---

## 1. Context & Background

### 1.1 Why This Task Exists

The existing `Window` (per `backend.md`) emits only `WindowResizeEvent` via
its GLFW resize callback. The rest of the engine — camera navigation,
future UI, future gameplay — needs keyboard, mouse-move, mouse-click, and
scroll input. This SDD adds the four input event types to `events.h` and
teaches `Window` how to bridge the corresponding GLFW callbacks into
user-provided `std::function` callbacks.

The callback-to-`EventBus` wiring is deliberately **not** done here — that
is `App`'s responsibility (SDD_05). `Window` only provides callback hooks;
the routing of those callbacks into `EventBus::emit` happens at composition
time.

- **Problem being solved**: The engine has no input surface beyond resize.
- **Relation to other modules**: Consumed by `CameraController` (SDD_03)
  via events routed through `EventBus`. Wired to `EventBus` by `App` (SDD_05).
- **Dependencies (upstream)**: GLFW (existing), `Window` (existing).
- **Dependents (downstream)**: SDD_03, SDD_05.

### 1.2 Reference Materials

| Resource | Path / URL | Notes |
|----------|-----------|-------|
| CONSTRAINTS.md | `./CONSTRAINTS.md` | Global hard constraints — frozen APIs, style |
| Existing Window | `core/window/Window.{h,cpp}` | Frozen signatures; extend additively |
| Existing events.h | `core/event/events.h` | Contains `WindowResizeEvent` — do not touch |
| Existing EventBus | `core/event/EventBus.h` | Read-only reference; App (SDD_05) will wire it |
| GLFW input docs | https://www.glfw.org/docs/latest/input_guide.html | Key codes, mouse buttons, scroll |

---

## 2. Specification

### 2.1 Objective

**Definition of Done**:
- [ ] Four new event structs + four new enum classes added to `events.h`.
- [ ] Existing `WindowResizeEvent` is byte-for-byte unchanged.
- [ ] `Window` emits all four input events through user-registered callbacks.
- [ ] `Window::get_aspect_ratio()` returns `width_ / static_cast<float>(height_)`
  with divide-by-zero guard (returns `1.0f`).
- [ ] Mouse-delta suppression on first callback (no spurious camera snap
  at startup).
- [ ] Unknown GLFW keys do not produce `KeyEvent`s.

### 2.2 Input / Output Contract

This is a system-level module — see §3.2 for data flow. There is no
pipeline-style I/O.

### 2.3 Functional Requirements

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-01 | `events.h` defines `KeyEvent`, `MouseMoveEvent`, `MouseClickEvent`, `ScrollEvent` as trivially copyable structs. | MUST |
| FR-02 | `events.h` defines `Key`, `KeyAction`, `Button`, `ClickAction` as `enum class` with the exact members shown in §3.3.1. | MUST |
| FR-03 | `Window::get_aspect_ratio()` returns `width_ / static_cast<float>(height_)`, returning `1.0f` when `height_ == 0`. | MUST |
| FR-04 | `Window::set_key_callback`, `set_mouse_move_callback`, `set_click_callback`, `set_scroll_callback` register user-provided `std::function`s that receive the corresponding event payload. | MUST |
| FR-05 | `Window`'s GLFW callbacks translate GLFW codes to SPATIUM enums; unknown GLFW keys result in no callback invocation. | MUST |
| FR-06 | First mouse-move callback after window creation does **not** invoke the user callback (`has_last_mouse_` flag suppresses the initial spurious delta). | MUST |
| FR-07 | Existing `WindowResizeEvent`, existing `Window` public methods, and the existing constructor signature (4 params including `vsync`) are unchanged. | MUST |
| FR-08 | No dependency on `EventBus` is introduced; `Window` does not `#include "core/event/EventBus.h"`. | MUST |

### 2.4 Non-Functional Requirements

| Category | Requirement |
|----------|-------------|
| Performance | Per-event cost dominated by one `std::function` invocation. Mouse-move callbacks fire at event rate (typically 60-500 Hz). |
| Memory | No heap allocation in callback path. |
| Compatibility | C++17, GLFW 3.0+. |
| Code Style | See CONSTRAINTS.md §3. |

---

## 3. Design

### 3.1 High-Level Approach

**Chosen approach**: Extend `events.h` with four POD structs + four `enum
class` types. Add four `std::function<void(const EventT&)>` callback
members to `Window`, with corresponding `set_*_callback` setters. Register
one static GLFW callback per input type; each static callback retrieves
the `Window*` via `glfwGetWindowUserPointer`, translates GLFW codes to
SPATIUM enums, and invokes the stored user callback if one is set.

**Rationale**:
- Matches the existing `WindowResizeEvent` pattern exactly — consistency
  across the `Window` class.
- Keeps `Window` dependency-light: no `EventBus` coupling, no knowledge
  of cameras, controllers, or the bus architecture.
- `std::function` indirection is measured in nanoseconds; negligible at
  input event rates.

**Rejected alternatives**:
- *Template-based callback registration* — rejected. Adds compile-time
  complexity with no measurable win; `std::function` is the right tool here.
- *Wire `EventBus` directly into `Window`* — rejected. Violates GD-02-style
  "single composition root" pattern. `Window` is a generic GLFW wrapper;
  tying it to `EventBus` would couple the two lifetimes permanently.
- *One omnibus callback taking an `std::variant<...>`* — rejected. Adds
  `std::variant` switch overhead to every consumer, and `std::variant`
  dispatch is less cache-friendly than four typed callbacks.

### 3.2 Architecture / Data Flow

```
User input (key press, mouse move, click, scroll)
    │
    ▼
GLFW (internal event queue)
    │
    ▼ (on glfwPollEvents)
static GLFW callback in Window.cpp
  e.g. glfw_key_callback(GLFWwindow* w, int key, int scancode,
                         int action, int mods)
    │
    ├─ glfwGetWindowUserPointer(w) → Window*
    │
    ▼
Window::on_key(glfw_key, glfw_action)    [instance method]
    │
    ├─ TranslateGlfwKey(glfw_key)    → std::optional<Key>
    │   (returns nullopt for unmapped keys)
    │
    ├─ TranslateGlfwAction(glfw_action) → KeyAction
    │
    ▼ (only if key was mapped AND user callback is set)
key_callback_(KeyEvent{key, action})
    │
    ▼ (user-provided lambda — outside this SDD's scope)
[Consumer does whatever it wants; SDD_05 will route to EventBus]
```

### 3.3 Key Interfaces / Implementations

#### 3.3.1 `events.h` additions

**File**: `core/event/events.h` (**extend**; the existing
`WindowResizeEvent` is untouched)

```cpp
#pragma once

// --- Existing — do NOT modify ---
struct WindowResizeEvent {
    int width;
    int height;
};

// --- New additions below ---

enum class Key {
    kW, kA, kS, kD,
    kUp, kDown, kLeft, kRight,
};

enum class KeyAction {
    kPress,
    kRelease,
    kRepeat,
};

enum class Button {
    kLeft,
    kRight,
    kMiddle,
};

enum class ClickAction {
    kPress,
    kRelease,
};

struct KeyEvent {
    Key       key;
    KeyAction action;
};

struct MouseMoveEvent {
    double dx;
    double dy;
};

struct MouseClickEvent {
    Button      button;
    ClickAction action;
    double      x;
    double      y;
};

struct ScrollEvent {
    double dy;
};
```

**Invariants**:
- All four event structs are trivially copyable — they travel by value
  through `std::function` invocations.
- Enum members use the `k`-prefix (see CONSTRAINTS.md §3).

#### 3.3.2 `Window` additions

**File**: `core/window/Window.h` / `Window.cpp` (**extend**; existing
methods and constructor are untouched)

```cpp
// -- Additions to Window.h public interface --

float get_aspect_ratio() const;

void set_key_callback        (std::function<void(const KeyEvent&)>        cb);
void set_mouse_move_callback (std::function<void(const MouseMoveEvent&)>  cb);
void set_click_callback      (std::function<void(const MouseClickEvent&)> cb);
void set_scroll_callback     (std::function<void(const ScrollEvent&)>     cb);

// -- Additions to Window.h private section --

std::function<void(const KeyEvent&)>        key_callback_;
std::function<void(const MouseMoveEvent&)>  mouse_move_callback_;
std::function<void(const MouseClickEvent&)> click_callback_;
std::function<void(const ScrollEvent&)>     scroll_callback_;

// First-frame mouse-delta suppression.
double last_mouse_x_ = 0.0;
double last_mouse_y_ = 0.0;
bool   has_last_mouse_ = false;

// Static GLFW callbacks.
static void glfw_key_callback   (GLFWwindow*, int key, int scancode,
                                                int action, int mods);
static void glfw_cursor_callback(GLFWwindow*, double x, double y);
static void glfw_mouse_callback (GLFWwindow*, int button, int action,
                                                int mods);
static void glfw_scroll_callback(GLFWwindow*, double x_off, double y_off);

// Instance-side handlers (called from static callbacks).
void on_key   (int glfw_key, int glfw_action);
void on_mouse (double x, double y);
void on_click (int glfw_button, int glfw_action, double x, double y);
void on_scroll(double dy);
```

#### 3.3.3 `get_aspect_ratio` implementation

```cpp
float Window::get_aspect_ratio() const {
    return (height_ > 0)
        ? static_cast<float>(width_) / static_cast<float>(height_)
        : 1.0f;
}
```

The `height_ == 0` guard matters: GLFW may emit a resize event with
`height == 0` when the user minimizes the window. Returning `1.0f` keeps
the projection matrix sane until the window is restored.

#### 3.3.4 GLFW translation helpers

In anonymous namespace inside `Window.cpp`:

```cpp
namespace {

std::optional<Key> TranslateGlfwKey(int glfw_key) {
    switch (glfw_key) {
        case GLFW_KEY_W:     return Key::kW;
        case GLFW_KEY_A:     return Key::kA;
        case GLFW_KEY_S:     return Key::kS;
        case GLFW_KEY_D:     return Key::kD;
        case GLFW_KEY_UP:    return Key::kUp;
        case GLFW_KEY_DOWN:  return Key::kDown;
        case GLFW_KEY_LEFT:  return Key::kLeft;
        case GLFW_KEY_RIGHT: return Key::kRight;
        default:             return std::nullopt;
    }
}

KeyAction TranslateGlfwAction(int glfw_action) {
    switch (glfw_action) {
        case GLFW_PRESS:   return KeyAction::kPress;
        case GLFW_RELEASE: return KeyAction::kRelease;
        case GLFW_REPEAT:  return KeyAction::kRepeat;
        default:           return KeyAction::kRelease;  // defensive fallback
    }
}

std::optional<Button> TranslateGlfwButton(int glfw_button) {
    switch (glfw_button) {
        case GLFW_MOUSE_BUTTON_LEFT:   return Button::kLeft;
        case GLFW_MOUSE_BUTTON_RIGHT:  return Button::kRight;
        case GLFW_MOUSE_BUTTON_MIDDLE: return Button::kMiddle;
        default:                       return std::nullopt;
    }
}

ClickAction TranslateGlfwClickAction(int glfw_action) {
    return (glfw_action == GLFW_PRESS) ? ClickAction::kPress
                                       : ClickAction::kRelease;
}

}  // namespace
```

#### 3.3.5 Instance-side handlers

```cpp
void Window::on_key(int glfw_key, int glfw_action) {
    auto key = TranslateGlfwKey(glfw_key);
    if (!key) return;                        // unknown key → drop
    if (!key_callback_) return;              // no subscriber → drop
    key_callback_(KeyEvent{*key, TranslateGlfwAction(glfw_action)});
}

void Window::on_mouse(double x, double y) {
    if (!has_last_mouse_) {
        last_mouse_x_ = x;
        last_mouse_y_ = y;
        has_last_mouse_ = true;
        return;                              // suppress first callback
    }
    double dx = x - last_mouse_x_;
    double dy = y - last_mouse_y_;
    last_mouse_x_ = x;
    last_mouse_y_ = y;
    if (mouse_move_callback_) {
        mouse_move_callback_(MouseMoveEvent{dx, dy});
    }
}

void Window::on_click(int glfw_button, int glfw_action,
                      double x, double y) {
    auto btn = TranslateGlfwButton(glfw_button);
    if (!btn) return;
    if (!click_callback_) return;
    click_callback_(MouseClickEvent{
        *btn, TranslateGlfwClickAction(glfw_action), x, y});
}

void Window::on_scroll(double dy) {
    if (scroll_callback_) {
        scroll_callback_(ScrollEvent{dy});
    }
}
```

#### 3.3.6 Callback registration (during Window construction)

The existing `Window` constructor already calls
`glfwSetWindowSizeCallback` and `glfwSetWindowUserPointer`. The new
callbacks are registered in the same place:

```cpp
// Appended to existing Window constructor body (exact location: after
// glfwSetWindowSizeCallback, before init_gl-style state setup):
glfwSetKeyCallback        (handle_, &Window::glfw_key_callback);
glfwSetCursorPosCallback  (handle_, &Window::glfw_cursor_callback);
glfwSetMouseButtonCallback(handle_, &Window::glfw_mouse_callback);
glfwSetScrollCallback     (handle_, &Window::glfw_scroll_callback);
```

The existing `glfwSetWindowUserPointer(handle_, this)` call already
supports retrieval from all four new static callbacks — no additional
plumbing needed.

### 3.4 File Structure Changes

```
SPATIUM/
└── core/
    ├── event/
    │   └── events.h                 ← MODIFY: append enums + event structs
    └── window/
        ├── Window.h                 ← MODIFY: add public setters + private members
        └── Window.cpp               ← MODIFY: implement handlers + GLFW bridges
```

No new files. No new dependencies. No new directories.

---

## 4. Implementation Plan

### 4.1 Task Breakdown

- [ ] **Step 1**: Extend `core/event/events.h` with enums + input event structs.
    - Expected result: Header compiles standalone; existing code still builds.
    - Completion signal: `WindowResizeEvent` diff is empty; `grep -c "struct"`
      in the file goes from 1 to 5.

- [ ] **Step 2**: Add the four public `set_*_callback` setters and their
  private callback members to `Window.h`.
    - Expected result: Window.h compiles.
    - Completion signal: `Window` class compiles; existing methods unchanged.

- [ ] **Step 3**: Implement `Window::get_aspect_ratio()` in `Window.cpp`.
    - Expected result: Compiles; `height_ == 0` path returns `1.0f`.
    - Completion signal: Unit test T-01 passes.

- [ ] **Step 4**: Add the four anonymous-namespace GLFW translation helpers
  to `Window.cpp`.
    - Expected result: Helpers compile; unmapped GLFW keys return `nullopt`.
    - Completion signal: Build succeeds.

- [ ] **Step 5**: Implement the four instance-side `on_*` handlers in
  `Window.cpp`.
    - Expected result: Handlers drop unmapped keys, suppress first mouse
      callback, invoke user callbacks when set.
    - Completion signal: Manual test — register printf lambdas for all
      four, interact with window, see output except on first mouse move.

- [ ] **Step 6**: Add the four static GLFW callbacks to `Window.cpp`. Each
  retrieves `Window*` via `glfwGetWindowUserPointer` and forwards to the
  matching `on_*` handler.
    - Expected result: Static callbacks compile and dispatch correctly.
    - Completion signal: `glfwSetKeyCallback(handle_, &Window::glfw_key_callback)`
      links without error.

- [ ] **Step 7**: Register the four new GLFW callbacks in the `Window`
  constructor, immediately after the existing `glfwSetWindowSizeCallback`
  call.
    - Expected result: All four callbacks fire when corresponding GLFW
      events arrive.
    - Completion signal: Test T-02 through T-06 pass.

- [ ] **Step 8**: Build at `-Wall -Wextra -Wpedantic`; run manual input
  regression.
    - Command: `cmake --build build`
    - Expected output: Zero warnings; WASD, mouse move, clicks, scroll all
      print from their registered test lambdas.

---

## 5. Testing & Validation

### 5.1 Test Cases

| Test ID | Description | Input | Expected Output | Pass Criteria |
|---------|-------------|-------|-----------------|---------------|
| T-01 | `get_aspect_ratio` normal | width=1280, height=720 | `≈1.7778` | `abs(r - 16.0/9.0) < 1e-5` |
| T-02 | `get_aspect_ratio` zero-height guard | width=1280, height=0 | `1.0f` | exact match |
| T-03 | Key event dispatch | Press `W` | `KeyEvent{kW, kPress}` delivered to callback | callback invoked once with correct payload |
| T-04 | Unknown GLFW key drop | Press `Escape` (not in `Key` enum) | No callback invocation | callback call count unchanged |
| T-05 | First mouse-move suppression | Initial `cursor_pos_callback` from GLFW | callback NOT invoked | call count == 0 after first event |
| T-06 | Subsequent mouse delta | Second `cursor_pos_callback` at (x+5, y+3) | `MouseMoveEvent{5, 3}` | dx/dy match |
| T-07 | Click event | Left-press at (100, 200) | `MouseClickEvent{kLeft, kPress, 100, 200}` | all 4 fields match |
| T-08 | Scroll event | `scroll_callback(_, 0, 1.5)` | `ScrollEvent{1.5}` | dy matches |
| T-09 | No callback set | Any input event before `set_*_callback` called | No crash, no effect | no null `std::function` invocation |
| T-10 | `WindowResizeEvent` unchanged | Grep the struct definition | Identical to pre-SDD state | `diff` produces no changes for that struct |

### 5.2 Validation Commands

```bash
# Build
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build

# Sanity grep
grep -A2 "struct WindowResizeEvent" core/event/events.h
# Expected: exact original definition unchanged.

# Manual run — register printf lambdas in a scratch main and verify output.
./build/SPATIUM
```

### 5.3 Acceptance Criteria

- [ ] All tests T-01 through T-10 pass.
- [ ] FR-01 through FR-08 verified.
- [ ] No modifications to CONSTRAINTS.md §1 frozen APIs.
- [ ] `WindowResizeEvent` byte-for-byte identical to pre-SDD state.
- [ ] Clean build at `-Wall -Wextra -Wpedantic`.
- [ ] §9 checklist walked through.

---

## 6. Constraints & Anti-Patterns

### 6.1 Hard Constraints (MUST NOT)

- ❌ Global constraints from **CONSTRAINTS.md** apply in full.
- ❌ Do NOT modify the existing `Window` constructor signature (4 params
  including `vsync`). Existing behavior preserved per GD decision D3-equivalent.
- ❌ Do NOT modify `WindowResizeEvent` in any way.
- ❌ Do NOT introduce `EventBus` as a dependency of `Window`. `Window` emits
  via `std::function`; routing to `EventBus` is SDD_05's job.
- ❌ Do NOT translate GLFW keys to integer-cast `Key` values; use the
  explicit `switch`-based helper. Integer casts break when GLFW renumbers
  or SPATIUM reorders the enum.
- ❌ Do NOT invoke user callbacks directly from the static GLFW callbacks;
  route through instance-side `on_*` handlers. The instance handlers are
  the proper place for translation, nullopt-check, and first-frame logic.

### 6.2 Known Pitfalls & Limitations

- ⚠️ **First-frame mouse snap**: without `has_last_mouse_`, the first
  `cursor_pos_callback` fires with `dx = x - 0.0`, which can be hundreds
  of pixels. `CameraController` would react by spinning the camera
  dramatically. The suppression is essential.
- ⚠️ **GLFW input polling**: input callbacks only fire during
  `glfwPollEvents()` (called in `App::run` each frame). If `App::run`
  is not yet wired to poll, no input callbacks will trigger — this is
  expected, not a Window bug.
- ⚠️ **`glfwGetWindowUserPointer` must be set before any callback**: the
  existing constructor already does this; just make sure the new
  `glfwSet*Callback` calls happen **after** the existing
  `glfwSetWindowUserPointer(handle_, this)` call.
- ⚠️ **Key repeat behavior**: holding a key fires `GLFW_PRESS` once, then
  `GLFW_REPEAT` continuously. `CameraController` consumers need to decide
  whether to treat `kRepeat` as continuous movement. This SDD exposes the
  distinction; consumer policy is out of scope.

| Limitation | Description | Severity | Possible Solution |
|---|---|---|---|
| Sparse key map | Only WASD + arrow keys mapped. No modifiers, no letters beyond WASD, no function keys. | Low | Extend `TranslateGlfwKey` when needed; each addition is mechanical. |
| No key-state polling | Consumers only receive edge-triggered events; no "is key currently down" query. | Low | Add a `std::unordered_set<Key> pressed_keys_` to `Window` if a consumer needs it (future SDD). |
| Scroll reports `dy` only | `xOffset` from GLFW discarded. | Low | Add `dx` field to `ScrollEvent` when horizontal scroll matters. |
| No raw mouse input | Uses `glfwSetCursorPosCallback` (cursor position), not raw motion. Sensitive to OS acceleration. | Low | Switch to `glfwSetInputMode(GLFW_RAW_MOUSE_MOTION, GLFW_TRUE)` when needed. |

---

## 7. Open Questions & Decision Log

### Open Questions

*(None — SDD ready for implementation.)*

### Decision Log

| # | Decision | Rationale | Date |
|---|----------|-----------|------|
| D1 | Kept `vsync` as the 4th `Window` constructor param (3-param variant in the class diagram is rejected). | Existing `Window` constructor is frozen per the user's directive not to alter implemented code. Diagram treated as logical shape, not ABI. | 2026-04-17 |
| D2 | Enum values use `k`-prefix per CONSTRAINTS.md §3. | Macro-collision-proof against `<windows.h>` and other system headers that `#define` bare identifiers. | 2026-04-17 |
| D3 | `Window` does NOT depend on `EventBus`; callback routing is `App`'s responsibility (SDD_05). | Keeps `Window` reusable and testable. Matches GD-02 "single composition root" stance. | 2026-04-17 |
| D4 | GLFW code translation uses explicit `switch`, not integer cast. | Enum value ordering is not a stable ABI contract; integer casts would break silently when either side reorders. | 2026-04-17 |
| D5 | First mouse-move callback is suppressed via `has_last_mouse_`. | Without this, the first `cursor_pos_callback` delta is `x - 0`, causing a large spurious camera rotation at startup. | 2026-04-17 |

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

### `events.h`
- [ ] `WindowResizeEvent` is unchanged (byte-identical).
- [ ] All four new enums use `enum class` (not plain `enum`).
- [ ] All four new event types are aggregate structs (no constructors).
- [ ] All enum values use the `k`-prefix.
- [ ] File includes only what it needs (no `<vector>`, `<functional>`, etc.).

### `Window.h`
- [ ] Existing public methods untouched.
- [ ] Existing constructor signature untouched (still 4 params).
- [ ] New members are all private.
- [ ] `has_last_mouse_` initialized to `false` in-class.
- [ ] No `#include "core/event/EventBus.h"`.

### `Window.cpp` — translation helpers
- [ ] `TranslateGlfwKey` uses explicit `switch`, returns `std::optional<Key>`.
- [ ] `TranslateGlfwKey` returns `nullopt` for any unmapped GLFW key.
- [ ] `TranslateGlfwAction` handles PRESS / RELEASE / REPEAT.
- [ ] `TranslateGlfwButton` uses `std::optional<Button>` for unknown buttons.
- [ ] All helpers in anonymous namespace (not `static` free functions).

### `Window.cpp` — handlers
- [ ] `on_key` early-returns on unknown key AND on unset callback.
- [ ] `on_mouse` suppresses first callback; subsequent calls compute delta correctly.
- [ ] `on_mouse` updates `last_mouse_x_` / `last_mouse_y_` AFTER computing delta.
- [ ] `on_click` early-returns on unknown button AND on unset callback.
- [ ] `on_scroll` only invokes callback if set.

### `Window.cpp` — GLFW static callbacks
- [ ] Each static callback retrieves `Window*` via `glfwGetWindowUserPointer`.
- [ ] Each static callback forwards to the instance `on_*` handler.
- [ ] No translation or filtering happens in the static layer — all logic in instance handlers.

### Constructor wiring
- [ ] The four new `glfwSet*Callback` calls happen AFTER `glfwSetWindowUserPointer`.
- [ ] The existing `glfwSetWindowSizeCallback` call is untouched.
- [ ] `glfwSetWindowUserPointer(handle_, this)` is called exactly once (existing call).

### `get_aspect_ratio`
- [ ] Returns `1.0f` when `height_ == 0`.
- [ ] Returns `float(width_) / float(height_)` otherwise.
- [ ] `const`-qualified.

### Build
- [ ] Compiles at `-Wall -Wextra -Wpedantic` with zero warnings.
- [ ] No new external dependencies added.

---

## 10. Appendix (Optional)

### 10.1 Related SDDs

- [CONSTRAINTS.md](./CONSTRAINTS.md) — global constraints.
- [SDD_03_Camera.md](./SDD_03_Camera.md) — primary consumer (`CameraController`
  subscribes to the four event types via `EventBus`).
- [SDD_05_AppIntegration.md](./SDD_05_AppIntegration.md) — wires
  `Window::set_*_callback` lambdas that call `EventBus::emit`.

### 10.2 Scratch Pad / Notes

- If a future SDD needs per-key state polling (rather than edge events),
  add an `std::unordered_set<Key> pressed_keys_` to `Window` and update
  `on_key` to maintain it. Trivial extension.
- If raw mouse motion is needed later, toggle via
  `glfwSetInputMode(handle_, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE)` during
  the rotation mode in `CameraController`.

---

## Outcome & Decisions

### Final Technical Choices

| Choice | Detail |
|--------|--------|
| Existing `events.h` content | File already had `WindowCloseEvent`, `KeyPressEvent`, `MouseButtonEvent`, `MouseScrollEvent`, `CursorMoveEvent` (raw GLFW-typed). All preserved; new typed enums+structs appended below them. |
| Existing resize callback | Uses `glfwSetFramebufferSizeCallback`, not `glfwSetWindowSizeCallback`. No change — new callbacks registered after it. |
| `std::function` callbacks | Used as specified; `std::move` in setters avoids unnecessary copy. |
| First-mouse suppression | Implemented via `has_last_mouse_ = false` in-class initializer, set true on first cursor event. |
| GLFW translation | Anonymous namespace in `Window.cpp`; `TranslateGlfwKey` returns `std::optional<Key>`, all others return concrete enum values. |
| Mouse click position | Retrieved via `glfwGetCursorPos` inside `glfw_mouse_callback` since GLFW does not pass cursor coords to the mouse-button callback. |

### Rejected Alternatives

- *Storing cursor position in Window and reading it in `on_click`*: rejected in favour of calling `glfwGetCursorPos` directly in the static callback — avoids an extra state field on `Window` and eliminates the one-frame lag.

### Notes for Future SDDs

- **SDD_05**: must call `window_.set_key_callback(...)`, `set_mouse_move_callback(...)`, etc. to wire `Window` events into `EventBus::emit`. `Window` has no knowledge of `EventBus`.
- **`events.h`** now contains two generations of event types: the raw GLFW-typed structs (`KeyPressEvent`, etc.) from before SDD_01, and the new typed enum structs (`KeyEvent`, etc.) from SDD_01. Consumers should use the new typed structs.

*End of SDD_01*