#pragma once

#include <GLFW/glfw3.h>
#include <cstdio>
#include <stdexcept>

struct GlfwContext {
    GlfwContext() {
        glfwSetErrorCallback([](int error, const char* desc) {
            fprintf(stderr, "GLFW Error %d: %s\n", error, desc);
        });
        if (!glfwInit())
            throw std::runtime_error("Failed to initialize GLFW");
    }

    ~GlfwContext() {
        glfwTerminate();
    }

    GlfwContext(const GlfwContext&)            = delete;
    GlfwContext& operator=(const GlfwContext&) = delete;
};
