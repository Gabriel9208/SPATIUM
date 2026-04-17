cmake_minimum_required(VERSION 3.15)
project(SPATIUM)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenGL REQUIRED)
find_package(glfw3  REQUIRED)
find_package(glm    REQUIRED)
find_package(glm CONFIG REQUIRED)

set(CORE_SOURCES
    core/glad.c
        core/gl/obj/EBO.cpp
        core/gl/obj/FBO.cpp
        core/gl/obj/UBO.cpp
        core/gl/obj/VAO.cpp
        core/gl/shader/ShaderProgram.cpp
        core/gl/shader/GraphicShader.cpp
        core/gl/shader/ComputeShader.cpp
        core/gl/obj/BufferObject.h
        core/gl/interfaces/IGPUResource.h
        core/gl/obj/VBO.h
        core/gl/obj/SSBO.h
        core/window/Window.h
        core/window/Window.cpp
        app/GlfwContext.h
        core/event/EventID.h
        core/event/EventBus.h
        core/event/events.h
        app/App.h
        app/App.cpp
        core/gl/debug/Debug.cpp
        core/gl/debug/Debug.h
        core/scene/Mesh.cpp
        core/scene/MeshManager.cpp
        core/scene/Scene.cpp
        core/camera/Camera.cpp
        core/camera/CameraController.cpp
        core/render/Renderer.cpp
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
