// tinyobjloader and tinyply single-header implementations — exactly one TU.
#define TINYOBJLOADER_IMPLEMENTATION
#include "tinyobjloader/tiny_obj_loader.h"

#define TINYPLY_IMPLEMENTATION
#include "tinyply/tinyply.h"

#include "core/scene/MeshManager.h"

#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/geometric.hpp>

// ---------------------------------------------------------------------------
// File-local helpers
// ---------------------------------------------------------------------------
namespace {

std::string ExtensionOfPath(const std::string& path) {
    std::string ext = std::filesystem::path(path).extension().string();
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

std::string StemOfPath(const std::string& path) {
    return std::filesystem::path(path).stem().string();
}

// Compute per-vertex averaged normals from indexed triangle soup.
// Called when the source file has no normals.
void compute_averaged_normals(std::vector<Vertex>& verts,
                              const std::vector<GLuint>& indices) {
    std::vector<glm::vec3> accumulated(verts.size(), glm::vec3(0.0f));
    for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
        const GLuint i0 = indices[i], i1 = indices[i + 1], i2 = indices[i + 2];
        const glm::vec3 e1 = verts[i1].position - verts[i0].position;
        const glm::vec3 e2 = verts[i2].position - verts[i0].position;
        glm::vec3 n = glm::cross(e1, e2);
        const float len = glm::length(n);
        if (len > 0.0f) n /= len;
        accumulated[i0] += n;
        accumulated[i1] += n;
        accumulated[i2] += n;
    }
    for (std::size_t i = 0; i < verts.size(); ++i) {
        const float len = glm::length(accumulated[i]);
        verts[i].normal = (len > 0.0f)
            ? accumulated[i] / len
            : glm::vec3(0.0f, 1.0f, 0.0f);  // default: world up
    }
}

// ---------------------------------------------------------------------------
// OBJ loader (tinyobjloader)
// One Vertex is created per unique (position_index, normal_index) pair.
// If the .obj has no normals, per-vertex averaged normals are computed after
// triangulation. — v1 limitation documented in SDD_02.
// ---------------------------------------------------------------------------
void LoadObjFromFile(const std::string& path,
                     std::vector<Vertex>& out_verts,
                     std::vector<GLuint>& out_indices) {
    tinyobj::ObjReaderConfig config;
    config.triangulate = true;

    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(path, config)) {
        throw std::runtime_error("LoadObjFromFile: " + reader.Error());
    }

    const auto& attrib = reader.GetAttrib();
    const auto& shapes = reader.GetShapes();
    const bool  has_normals = !attrib.normals.empty();

    // Build non-deduplicated vertex list: one Vertex per face-corner.
    // This avoids any hash-map complexity and is correct for GL rendering.
    out_verts.reserve(shapes.size() * 3);
    out_indices.reserve(shapes.size() * 3);

    for (const auto& shape : shapes) {
        for (const auto& idx : shape.mesh.indices) {
            Vertex v;
            v.position = {
                attrib.vertices[3 * idx.vertex_index + 0],
                attrib.vertices[3 * idx.vertex_index + 1],
                attrib.vertices[3 * idx.vertex_index + 2],
            };
            if (has_normals && idx.normal_index >= 0) {
                v.normal = {
                    attrib.normals[3 * idx.normal_index + 0],
                    attrib.normals[3 * idx.normal_index + 1],
                    attrib.normals[3 * idx.normal_index + 2],
                };
            } else {
                v.normal = glm::vec3(0.0f);
            }
            out_indices.push_back(static_cast<GLuint>(out_verts.size()));
            out_verts.push_back(v);
        }
    }

    if (!has_normals && !out_verts.empty()) {
        compute_averaged_normals(out_verts, out_indices);
    }
}

// ---------------------------------------------------------------------------
// PLY loader (tinyply)
// Supports ascii and binary_little_endian. Only triangulated faces.
// Quads / NGons throw std::runtime_error. — v1 limitation per SDD_02.
// ---------------------------------------------------------------------------

// Maps a tinyply::Type to its byte width.
static std::size_t ply_type_stride(tinyply::Type t) {
    switch (t) {
        case tinyply::Type::INT8:    case tinyply::Type::UINT8:   return 1;
        case tinyply::Type::INT16:   case tinyply::Type::UINT16:  return 2;
        case tinyply::Type::INT32:   case tinyply::Type::UINT32:  return 4;
        case tinyply::Type::FLOAT32: return 4;
        case tinyply::Type::FLOAT64: return 8;
        default:                     return 0;
    }
}

void LoadPlyFromFile(const std::string& path,
                     std::vector<Vertex>& out_verts,
                     std::vector<GLuint>& out_indices) {
    std::ifstream ss(path, std::ios::binary);
    if (!ss.is_open()) {
        throw std::runtime_error("LoadPlyFromFile: cannot open '" + path + "'");
    }

    tinyply::PlyFile file;
    file.parse_header(ss);

    // Request vertex positions (required).
    std::shared_ptr<tinyply::PlyData> verts_data;
    try {
        verts_data = file.request_properties_from_element("vertex", {"x", "y", "z"});
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("LoadPlyFromFile: no vertex positions: ") + e.what());
    }

    // Request normals (optional).
    std::shared_ptr<tinyply::PlyData> normals_data;
    bool has_normals = false;
    try {
        normals_data = file.request_properties_from_element("vertex", {"nx", "ny", "nz"});
        has_normals = true;
    } catch (const std::exception&) {
        // No normals in file — will compute them after loading.
    }

    // Request face indices. Try both common property names.
    std::shared_ptr<tinyply::PlyData> faces_data;
    try {
        faces_data = file.request_properties_from_element("face", {"vertex_indices"});
    } catch (const std::exception&) {
        try {
            faces_data = file.request_properties_from_element("face", {"vertex_index"});
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("LoadPlyFromFile: no face indices: ") + e.what());
        }
    }

    file.read(ss);

    // --- Vertex positions ---
    const std::size_t num_verts = verts_data->count;
    out_verts.resize(num_verts);

    {
        const std::size_t stride = ply_type_stride(verts_data->t);
        const uint8_t*    buf    = verts_data->buffer.get();
        for (std::size_t i = 0; i < num_verts; ++i) {
            float xyz[3];
            for (int c = 0; c < 3; ++c) {
                const uint8_t* src = buf + (i * 3 + c) * stride;
                if (verts_data->t == tinyply::Type::FLOAT32) {
                    std::memcpy(&xyz[c], src, 4);
                } else if (verts_data->t == tinyply::Type::FLOAT64) {
                    double d;
                    std::memcpy(&d, src, 8);
                    xyz[c] = static_cast<float>(d);
                } else {
                    throw std::runtime_error("LoadPlyFromFile: vertex positions must be float");
                }
            }
            out_verts[i].position = {xyz[0], xyz[1], xyz[2]};
            out_verts[i].normal   = glm::vec3(0.0f);
        }
    }

    // --- Normals (optional) ---
    if (has_normals && normals_data && normals_data->t == tinyply::Type::FLOAT32) {
        const float* nrm = reinterpret_cast<const float*>(normals_data->buffer.get());
        for (std::size_t i = 0; i < num_verts; ++i) {
            out_verts[i].normal = {nrm[i * 3 + 0], nrm[i * 3 + 1], nrm[i * 3 + 2]};
        }
    } else {
        has_normals = false;  // fallback to computation
    }

    // --- Face indices ---
    // tinyply two-pass mode (list_size_hint=0): buffer contains all index
    // values concatenated WITHOUT per-face count bytes.
    {
        const std::size_t idx_stride   = ply_type_stride(faces_data->t);
        if (idx_stride == 0) {
            throw std::runtime_error("LoadPlyFromFile: unsupported face index type");
        }
        const std::size_t total_items  = faces_data->buffer.size_bytes() / idx_stride;
        const std::size_t num_faces    = faces_data->count;

        if (total_items != num_faces * 3) {
            throw std::runtime_error(
                "LoadPlyFromFile: only triangular faces are supported "
                "(got " + std::to_string(total_items / num_faces) + " vertices per face)");
        }

        out_indices.resize(total_items);
        const uint8_t* buf = faces_data->buffer.get();
        for (std::size_t i = 0; i < total_items; ++i) {
            GLuint idx = 0;
            switch (faces_data->t) {
                case tinyply::Type::INT32:  { int32_t  v; std::memcpy(&v, buf, 4); idx = static_cast<GLuint>(v); buf += 4; break; }
                case tinyply::Type::UINT32: { uint32_t v; std::memcpy(&v, buf, 4); idx = v;                      buf += 4; break; }
                case tinyply::Type::INT16:  { int16_t  v; std::memcpy(&v, buf, 2); idx = static_cast<GLuint>(v); buf += 2; break; }
                case tinyply::Type::UINT16: { uint16_t v; std::memcpy(&v, buf, 2); idx = v;                      buf += 2; break; }
                case tinyply::Type::INT8:   { int8_t   v; std::memcpy(&v, buf, 1); idx = static_cast<GLuint>(v); buf += 1; break; }
                case tinyply::Type::UINT8:  { uint8_t  v; std::memcpy(&v, buf, 1); idx = v;                      buf += 1; break; }
                default: throw std::runtime_error("LoadPlyFromFile: unsupported face index type");
            }
            out_indices[i] = idx;
        }
    }

    if (!has_normals && !out_verts.empty()) {
        compute_averaged_normals(out_verts, out_indices);
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// MeshManager implementation
// ---------------------------------------------------------------------------

int MeshManager::create_mesh(const std::string& name,
                             const std::vector<Vertex>& vertices,
                             const std::vector<GLuint>& indices) {
    if (vertices.empty()) {
        throw std::runtime_error("create_mesh: vertices empty for '" + name + "'");
    }
    if (ids_.count(name) != 0) {
        throw std::runtime_error("create_mesh: duplicate name '" + name + "'");
    }

    // VAO constructor calls glGenVertexArrays + glBindVertexArray immediately.
    VAO         vao;
    VBO<Vertex> vbo(vertices, GL_STATIC_DRAW);
    // EBO constructor calls glCreateBuffers + glBindBuffer while VAO is bound.
    EBO         ebo(indices);

    // Explicit setup block for attribute pointers. VAO is already bound but
    // calling bind() again is harmless and makes the intent clear.
    vao.bind();
    vbo.bind();
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                          sizeof(Vertex),
                          // NOLINTNEXTLINE(performance-no-int-to-ptr)
                          (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                          sizeof(Vertex),
                          // NOLINTNEXTLINE(performance-no-int-to-ptr)
                          (void*)offsetof(Vertex, normal));
    ebo.bind();
    vao.unbind();

    const int id = next_id_++;
    meshes_.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(id),
        std::forward_as_tuple(id, name,
                              std::move(vao),
                              std::move(vbo),
                              std::move(ebo),
                              indices.size()));
    ids_[name]  = id;
    names_[id]  = name;
    return id;
}

int MeshManager::load_mesh(const std::string& filename,
                           std::optional<std::string> name) {
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;

    const std::string ext = ExtensionOfPath(filename);
    if (ext == ".obj") {
        LoadObjFromFile(filename, vertices, indices);
    } else if (ext == ".ply") {
        LoadPlyFromFile(filename, vertices, indices);
    } else {
        throw std::runtime_error("load_mesh: unsupported extension '" + ext + "'");
    }

    const std::string resolved = name.value_or(StemOfPath(filename));
    return create_mesh(resolved, vertices, indices);
}

Mesh& MeshManager::get_by_id(int id) {
    auto it = meshes_.find(id);
    if (it == meshes_.end()) {
        throw std::out_of_range("MeshManager::get_by_id: id " + std::to_string(id) + " not found");
    }
    return it->second;
}

const Mesh& MeshManager::get_by_id(int id) const {
    auto it = meshes_.find(id);
    if (it == meshes_.end()) {
        throw std::out_of_range("MeshManager::get_by_id: id " + std::to_string(id) + " not found");
    }
    return it->second;
}

Mesh& MeshManager::get_by_name(const std::string& name) {
    auto it = ids_.find(name);
    if (it == ids_.end()) {
        throw std::out_of_range("MeshManager::get_by_name: '" + name + "' not found");
    }
    return meshes_.at(it->second);
}

const Mesh& MeshManager::get_by_name(const std::string& name) const {
    auto it = ids_.find(name);
    if (it == ids_.end()) {
        throw std::out_of_range("MeshManager::get_by_name: '" + name + "' not found");
    }
    return meshes_.at(it->second);
}

bool MeshManager::contains(int id) const noexcept {
    return meshes_.count(id) != 0;
}

bool MeshManager::contains(const std::string& name) const noexcept {
    return ids_.count(name) != 0;
}
