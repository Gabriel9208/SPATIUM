#include "core/render/Renderer.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

// ---------------------------------------------------------------------------
// clear
// ---------------------------------------------------------------------------

void Renderer::clear() const {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

// ---------------------------------------------------------------------------
// upload_frame_uniforms_ — camera-derived state, once per draw() call
// ---------------------------------------------------------------------------

void Renderer::upload_frame_uniforms_(const Camera&        camera,
                                      const GraphicShader& shader) const {
    // GraphicShader::use() is const in the frozen API — no cast needed.
    shader.use();
    const GLuint prog = shader.get_id();

    if (GLint loc = glGetUniformLocation(prog, "uView"); loc != -1) {
        glUniformMatrix4fv(loc, 1, GL_FALSE,
                           glm::value_ptr(camera.get_view()));
    }
    if (GLint loc = glGetUniformLocation(prog, "uProjection"); loc != -1) {
        glUniformMatrix4fv(loc, 1, GL_FALSE,
                           glm::value_ptr(camera.get_projection()));
    }
    if (GLint loc = glGetUniformLocation(prog, "uViewPos"); loc != -1) {
        const glm::vec3 pos = camera.position();
        glUniform3fv(loc, 1, glm::value_ptr(pos));
    }
}

// ---------------------------------------------------------------------------
// upload_mesh_uniforms_ — per-mesh state, once per mesh
// ---------------------------------------------------------------------------

void Renderer::upload_mesh_uniforms_(const Mesh&          /*mesh*/,
                                     const GraphicShader& shader) const {
    // v1: Mesh has no Transform yet — upload identity.
    // When Transform is added (follow-up SDD), replace with:
    //   const glm::mat4 model = mesh.transform();
    //   const glm::mat3 normal = glm::transpose(glm::inverse(glm::mat3(model)));
    static const glm::mat4 kIdentityMat4 = glm::mat4(1.0f);
    static const glm::mat3 kIdentityMat3 = glm::mat3(1.0f);

    const GLuint prog = shader.get_id();

    if (GLint loc = glGetUniformLocation(prog, "uModel"); loc != -1) {
        glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(kIdentityMat4));
    }
    if (GLint loc = glGetUniformLocation(prog, "uNormalMatrix"); loc != -1) {
        glUniformMatrix3fv(loc, 1, GL_FALSE, glm::value_ptr(kIdentityMat3));
    }
}

// ---------------------------------------------------------------------------
// draw — single mesh
// ---------------------------------------------------------------------------

void Renderer::draw(const Camera&        camera,
                    const Mesh&          mesh,
                    const GraphicShader& shader) const {
    upload_frame_uniforms_(camera, shader);
    upload_mesh_uniforms_(mesh, shader);
    mesh.bind();
    glDrawElements(GL_TRIANGLES,
                   static_cast<GLsizei>(mesh.get_index_count()),
                   GL_UNSIGNED_INT, nullptr);
}

// ---------------------------------------------------------------------------
// draw — scene (frame uniforms uploaded once, before mesh loop)
// ---------------------------------------------------------------------------

void Renderer::draw(const Camera&        camera,
                    const Scene&         scene,
                    const GraphicShader& shader) const {
    upload_frame_uniforms_(camera, shader);
    for (const Mesh* mesh : scene) {
        if (!mesh) continue;   // FR-09: defensive null-skip
        upload_mesh_uniforms_(*mesh, shader);
        mesh->bind();
        glDrawElements(GL_TRIANGLES,
                       static_cast<GLsizei>(mesh->get_index_count()),
                       GL_UNSIGNED_INT, nullptr);
    }
}
