#pragma once

struct WindowResizeEvent {
    int width;
    int height;
};

struct WindowCloseEvent {};

struct KeyPressEvent {
    int key;
    int scancode;
    int action;
    int mods;
};

struct MouseButtonEvent {
    int button;
    int action;
    int mods;
};

struct MouseScrollEvent {
    double xoffset;
    double yoffset;
};

struct CursorMoveEvent {
    double x;
    double y;
};

// ---------------------------------------------------------------------------
// Typed input events (SDD_01)
// ---------------------------------------------------------------------------

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
