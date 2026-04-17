#pragma once
#include "core/camera/Camera.h"
#include "core/scene/Mesh.h"
#include "core/scene/Scene.h"
#include "core/gl/shader/GraphicShader.h"

// Renderer — stateless coordinator that clears the framebuffer, uploads
// camera state as frame-scope uniforms, iterates a Scene uploading
// per-mesh uniforms, and issues draw calls.
//
// Design (GD-05): uploads the 5-uniform contract defined in CONSTRAINTS.md.
// Missing uniforms (glGetUniformLocation == -1) are silently skipped.
// No GL resources are owned; no heap allocation in the draw path.
class Renderer {
public:
    Renderer() = default;
    ~Renderer() = default;

    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;

    void clear() const;

    void draw(const Camera&       camera,
              const Mesh&         mesh,
              const GraphicShader& shader) const;

    void draw(const Camera&       camera,
              const Scene&        scene,
              const GraphicShader& shader) const;

private:
    // Frame-scope uniforms — uploaded once per draw() call.
    void upload_frame_uniforms_(const Camera&       camera,
                                const GraphicShader& shader) const;

    // Mesh-scope uniforms — uploaded once per mesh. v1: identity values.
    void upload_mesh_uniforms_(const Mesh&         mesh,
                               const GraphicShader& shader) const;
};
