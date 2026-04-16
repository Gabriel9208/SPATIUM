cmake_minimum_required(VERSION 3.15)
project(SPATIUM)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenGL REQUIRED)
find_package(glfw3  REQUIRED)
find_package(glm    REQUIRED)
find_package(glm CONFIG REQUIRED)

set(CORE_SOURCES
    core/glad.c
        core/globj/EBO.cpp
        core/globj/FBO.cpp
        core/globj/UBO.cpp
        core/globj/VAO.cpp
        core/globj/shader_program/ShaderProgram.cpp
        core/globj/shader_program/GraphicShader.cpp
        core/globj/shader_program/ComputeShader.cpp
        core/globj/BufferObject.h
        core/globj/interfaces/IGPUResource.h
        core/globj/VBO.h
        core/globj/SSBO.h
        core/window/Window.h
        core/window/Window.cpp
        app/GlfwContext.h
        core/event/EventID.h
        core/event/EventBus.h
        core/event/events.h
        app/App.h
        app/App.cpp
        core/globj/debug/Debug.cpp
        core/globj/debug/Debug.h
)

add_executable(SPATIUM
    main.cpp
    ${CORE_SOURCES}
)

target_include_directories(SPATIUM PRIVATE
        ${CMAKE_SOURCE_DIR}          # lets Core files include each other as "Core/..."
        ${CMAKE_SOURCE_DIR}/include   # vendored GLAD headers (glad/glad.h, KHR/khrplatform.h)
        ${CMAKE_SOURCE_DIR}/external SYSTEM  # vendored GLAD headers (glad/glad.h, KHR/khrplatform.h)
)

target_link_libraries(SPATIUM PRIVATE
    glfw
    OpenGL::GL
    glm::glm
)
