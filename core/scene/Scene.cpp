#include "core/scene/Scene.h"

Scene::Scene(Mesh& mesh) {
    meshes_.push_back(&mesh);
}

Scene::Scene(const std::vector<Mesh*>& meshes)
    : meshes_(meshes)
{}

void Scene::add_mesh(Mesh* mesh) {
    assert(mesh != nullptr && "Scene::add_mesh does not accept nullptr");
    meshes_.push_back(mesh);
}
