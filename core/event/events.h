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
