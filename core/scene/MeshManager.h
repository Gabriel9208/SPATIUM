#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <glad/glad.h>

#include "core/scene/Mesh.h"

class MeshManager {
public:
    MeshManager()  = default;
    ~MeshManager() = default;

    MeshManager(const MeshManager&)            = delete;
    MeshManager& operator=(const MeshManager&) = delete;

    // Load from disk. Extension determines the parser (.obj → tinyobjloader,
    // .ply → tinyply). Default name is the filename stem.
    // Throws std::runtime_error on I/O failure, unsupported extension, or
    // duplicate name.
    int load_mesh(const std::string& filename,
                  std::optional<std::string> name = std::nullopt);

    // Create from in-memory data. This is the ONLY function that configures
    // VAO attribute pointers. load_mesh is implemented in terms of this.
    // Throws std::runtime_error on duplicate name or empty vertices.
    int create_mesh(const std::string& name,
                    const std::vector<Vertex>& vertices,
                    const std::vector<GLuint>& indices);

    Mesh&       get_by_id(int id);
    const Mesh& get_by_id(int id) const;

    Mesh&       get_by_name(const std::string& name);
    const Mesh& get_by_name(const std::string& name) const;

    bool contains(int id)                  const noexcept;
    bool contains(const std::string& name) const noexcept;

private:
    std::unordered_map<int, Mesh>        meshes_;
    std::unordered_map<std::string, int> ids_;    // name → id
    std::unordered_map<int, std::string> names_;  // id   → name
    int next_id_ = 0;
};
