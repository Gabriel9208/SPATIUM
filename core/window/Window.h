#pragma once

#include <functional>
#include <string>

#include <GLFW/glfw3.h>

class Window {
public:
    explicit Window(int width, int height, std::string title = "SPATIUM", bool vsync = true);
    ~Window();

    Window(const Window&)            = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&)                 = delete;
    Window& operator=(Window&&)      = delete;

    [[nodiscard]] bool should_close()         const { return glfwWindowShouldClose(handle_); }
    void               swap_buffers()         const { glfwSwapBuffers(handle_); }
    void               make_context_current() const { glfwMakeContextCurrent(handle_); }

    [[nodiscard]] int  get_width()  const { return width_; }
    [[nodiscard]] int  get_height() const { return height_; }

    void set_resize_callback(std::function<void(int, int)> cbf);

private:
    GLFWwindow*                    handle_         = nullptr;
    std::string                    title_;
    int                            width_          = 0;
    int                            height_         = 0;
    std::function<void(int, int)>  resize_callback_;

    static void glfw_resize_callback(GLFWwindow* win, int w, int h);
    void        on_resize(int w, int h);
};
