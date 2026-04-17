#pragma once

#include <cassert>
#include <cstddef>
#include <vector>

#include "core/scene/Mesh.h"

// Scene is a non-owning view of a subset of Mesh objects.
// Invariant: every MeshManager that owns the referenced Mesh instances MUST
// outlive all Scene instances that reference them. (CONSTRAINTS.md GD-01)
class Scene {
public:
    Scene() = default;

    // Construct with a single mesh reference.
    explicit Scene(Mesh& mesh);

    // Construct from a vector of non-owning pointers.
    explicit Scene(const std::vector<Mesh*>& meshes);

    Scene(const Scene&)            = delete;
    Scene& operator=(const Scene&) = delete;

    // Non-owning. Asserts non-null in debug builds.
    void add_mesh(Mesh* mesh);

    [[nodiscard]] std::vector<Mesh*>::const_iterator begin() const noexcept { return meshes_.begin(); }
    [[nodiscard]] std::vector<Mesh*>::const_iterator end()   const noexcept { return meshes_.end();   }
    [[nodiscard]] std::size_t                        size()  const noexcept { return meshes_.size();  }

private:
    std::vector<Mesh*> meshes_;
};
