#include "LibMeshbIO.h"

#include <libmeshb7.h>
#include <spdlog/spdlog.h>

// VTK Cell Type Constants
constexpr unsigned char VTK_TRIANGLE = 5;
constexpr unsigned char VTK_QUAD = 9;
constexpr unsigned char VTK_TETRA = 10;
constexpr unsigned char VTK_HEXAHEDRON = 12;
constexpr unsigned char VTK_WEDGE = 13;
constexpr unsigned char VTK_PYRAMID = 14;

namespace {
template <int V>
using MeshInt_t = std::conditional_t<(V <= 3), int, int64_t>;
template <int V>
using MeshDbl_t = std::conditional_t<(V <= 2), float, double>;

template<typename MI>
void add_face(MeshData& mesh_data, std::initializer_list<MI> vertices)
{
    for (auto v : vertices) {
        mesh_data.face_vertices_.push_back(static_cast<Index>(v - 1)); // Convert to 0-based
    }
    mesh_data.face_vertices_offset_.push_back(static_cast<Index>(mesh_data.face_vertices_.size()));
};

template <typename MI>
void add_solid(MeshData& mesh_data, unsigned char type, std::initializer_list<MI> vertices)
{
    mesh_data.solid_types_.push_back(type);
    for (auto v : vertices) {
        mesh_data.solid_vertices_.push_back(static_cast<Index>(v - 1)); // Convert to 0-based
    }
    mesh_data.solid_vertices_offset_.push_back(static_cast<Index>(mesh_data.solid_vertices_.size()));
    mesh_data.solid_faces_offset_.push_back(0);
};

template <int V>
void read_given_version(MeshData& mesh_data, int64_t meshb_idx)
{
    static_assert(0 <= V && V <= 4 && "Version 0~4 LibMeshb supported");
    using MeshInt = MeshInt_t<V>;
    using MeshDbl = MeshDbl_t<V>;

    mesh_data.init();

    // Read Vertices
    int64_t num_vertices = GmfStatKwd(meshb_idx, GmfVertices);
    if (num_vertices > 0) {
        mesh_data.vertex_positions_.reserve(num_vertices);
        GmfGotoKwd(meshb_idx, GmfVertices);

        for (int64_t i = 0; i < num_vertices; ++i) {
            MeshDbl x, y, z;
            MeshInt ref;
            GmfGetLin(meshb_idx, GmfVertices, &x, &y, &z, &ref);
            mesh_data.vertex_positions_.push_back({ x, y, z });
        }
        spdlog::info("Read {} vertices", num_vertices);
    }

    // Read Edges
    int64_t num_edges = GmfStatKwd(meshb_idx, GmfEdges);
    if (num_edges > 0) {
        GmfGotoKwd(meshb_idx, GmfEdges);
        mesh_data.edge_vertices_.reserve(num_edges * 2);

        for (int64_t i = 0; i < num_edges; ++i) {
            MeshInt v1, v2, ref;
            GmfGetLin(meshb_idx, GmfEdges, &v1, &v2, &ref);
            // Convert from 1-based to 0-based indexing
            mesh_data.edge_vertices_.push_back(static_cast<Index>(v1 - 1));
            mesh_data.edge_vertices_.push_back(static_cast<Index>(v2 - 1));
        }
        spdlog::info("Read {} edges", num_edges);
    }

    // Read Triangles
    int64_t num_triangles = GmfStatKwd(meshb_idx, GmfTriangles);
    if (num_triangles > 0) {
        GmfGotoKwd(meshb_idx, GmfTriangles);
        for (int64_t i = 0; i < num_triangles; ++i) {
            MeshInt v1, v2, v3, ref;
            GmfGetLin(meshb_idx, GmfTriangles, &v1, &v2, &v3, &ref);
            add_face(mesh_data, { v1, v2, v3 });
        }
        spdlog::info("Read {} triangles", num_triangles);
    }

    // Read Quadrilaterals
    int64_t num_quads = GmfStatKwd(meshb_idx, GmfQuadrilaterals);
    if (num_quads > 0) {
        GmfGotoKwd(meshb_idx, GmfQuadrilaterals);
        for (int64_t i = 0; i < num_quads; ++i) {
            MeshInt v1, v2, v3, v4, ref;
            GmfGetLin(meshb_idx, GmfQuadrilaterals, &v1, &v2, &v3, &v4, &ref);
            add_face(mesh_data, { v1, v2, v3, v4 });
        }
        spdlog::info("Read {} quadrilaterals", num_quads);
    }

    // Read Tetrahedra
    int64_t num_tets = GmfStatKwd(meshb_idx, GmfTetrahedra);
    if (num_tets > 0) {
        GmfGotoKwd(meshb_idx, GmfTetrahedra);
        for (int64_t i = 0; i < num_tets; ++i) {
            MeshInt v1, v2, v3, v4, ref;
            GmfGetLin(meshb_idx, GmfTetrahedra, &v1, &v2, &v3, &v4, &ref);
            add_solid(mesh_data, VTK_TETRA, { v1, v2, v3, v4 });
        }
        spdlog::info("Read {} tetrahedra", num_tets);
    }

    // Read Pyramids
    int64_t num_pyramids = GmfStatKwd(meshb_idx, GmfPyramids);
    if (num_pyramids > 0) {
        GmfGotoKwd(meshb_idx, GmfPyramids);
        for (int64_t i = 0; i < num_pyramids; ++i) {
            MeshInt v1, v2, v3, v4, v5, ref;
            GmfGetLin(meshb_idx, GmfPyramids, &v1, &v2, &v3, &v4, &v5, &ref);
            add_solid(mesh_data, VTK_PYRAMID, { v1, v2, v3, v4, v5 });
        }
        spdlog::info("Read {} pyramids", num_pyramids);
    }

    // Read Prisms (Wedges)
    int64_t num_prisms = GmfStatKwd(meshb_idx, GmfPrisms);
    if (num_prisms > 0) {
        GmfGotoKwd(meshb_idx, GmfPrisms);
        for (int64_t i = 0; i < num_prisms; ++i) {
            MeshInt v1, v2, v3, v4, v5, v6, ref;
            GmfGetLin(meshb_idx, GmfPrisms, &v1, &v2, &v3, &v4, &v5, &v6, &ref);
            add_solid(mesh_data, VTK_WEDGE, { v1, v2, v3, v4, v5, v6 });
        }
        spdlog::info("Read {} prisms", num_prisms);
    }

    // Read Hexahedra
    int64_t num_hexes = GmfStatKwd(meshb_idx, GmfHexahedra);
    if (num_hexes > 0) {
        GmfGotoKwd(meshb_idx, GmfHexahedra);
        for (int64_t i = 0; i < num_hexes; ++i) {
            MeshInt v1, v2, v3, v4, v5, v6, v7, v8, ref;
            GmfGetLin(meshb_idx, GmfHexahedra, &v1, &v2, &v3, &v4, &v5, &v6, &v7, &v8, &ref);
            add_solid(mesh_data, VTK_HEXAHEDRON, { v1, v2, v3, v4, v5, v6, v7, v8 });
        }
        spdlog::info("Read {} hexahedra", num_hexes);
    }
}
}

bool LibMeshbIO::read(const std::filesystem::path& input_path, MeshData& mesh_data)
{
    int version = 1;
    int dimension = 3;
    int64_t meshb_idx = GmfOpenMesh(input_path.string().c_str(), GmfRead, &version, &dimension);
    if (!meshb_idx) {
        spdlog::error("Invalid mesh index");
        return false;
    }

    try {
        switch (version) {
        case 1:
            read_given_version<1>(mesh_data, meshb_idx);
            break;
        case 2:
        case 3:
            read_given_version<3>(mesh_data, meshb_idx);
            break;
        case 4:
            read_given_version<4>(mesh_data, meshb_idx);
            break;
        default:
            spdlog::error("Unsupported mesh version: {}", version);
        }

        GmfCloseMesh(meshb_idx);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Error reading mesh data: {}", e.what());
        GmfCloseMesh(meshb_idx);
        return false;
    }
}

bool LibMeshbIO::write(int64_t meshb_idx, const MeshData& mesh_data)
{
    if (!meshb_idx) {
        spdlog::error("Invalid mesh index");
        return false;
    }

    try {
        // Write Vertices
        int64_t num_vertices = mesh_data.vertex_positions_.size();
        if (num_vertices > 0) {
            GmfSetKwd(meshb_idx, GmfVertices, num_vertices);
            for (int64_t i = 0; i < num_vertices; ++i) {
                GmfSetLin(meshb_idx, GmfVertices,
                    static_cast<float>(mesh_data.vertex_positions_[i][0]),
                    static_cast<float>(mesh_data.vertex_positions_[i][1]),
                    static_cast<float>(mesh_data.vertex_positions_[i][2]),
                    0); // reference = 0
            }
            spdlog::info("Wrote {} vertices", num_vertices);
        }

        // Write Edges
        int64_t num_edges = mesh_data.edge_vertices_.size() / 2;
        if (num_edges > 0) {
            GmfSetKwd(meshb_idx, GmfEdges, num_edges);
            for (int64_t i = 0; i < num_edges; ++i) {
                GmfSetLin(meshb_idx, GmfEdges,
                    mesh_data.edge_vertices_[i * 2] + 1, // Convert to 1-based
                    mesh_data.edge_vertices_[i * 2 + 1] + 1,
                    0); // reference = 0
            }
            spdlog::info("Wrote {} edges", num_edges);
        }

        // Separate faces by type for writing
        std::vector<std::array<int, 3>> triangles;
        std::vector<std::array<int, 4>> quads;

        for (size_t i = 0; i + 1 < mesh_data.face_vertices_offset_.size(); ++i) {
            Index start = mesh_data.face_vertices_offset_[i];
            Index end = mesh_data.face_vertices_offset_[i + 1];
            Index num_face_vertices = end - start;

            if (num_face_vertices == 3) {
                triangles.push_back({ mesh_data.face_vertices_[start] + 1, // Convert to 1-based
                    mesh_data.face_vertices_[start + 1] + 1,
                    mesh_data.face_vertices_[start + 2] + 1 });
            } else if (num_face_vertices == 4) {
                quads.push_back({ mesh_data.face_vertices_[start] + 1, // Convert to 1-based
                    mesh_data.face_vertices_[start + 1] + 1,
                    mesh_data.face_vertices_[start + 2] + 1,
                    mesh_data.face_vertices_[start + 3] + 1 });
            }
        }

        // Write Triangles
        if (!triangles.empty()) {
            GmfSetKwd(meshb_idx, GmfTriangles, triangles.size());
            for (const auto& tri : triangles) {
                GmfSetLin(meshb_idx, GmfTriangles, tri[0], tri[1], tri[2], 0);
            }
            spdlog::info("Wrote {} triangles", triangles.size());
        }

        // Write Quadrilaterals
        if (!quads.empty()) {
            GmfSetKwd(meshb_idx, GmfQuadrilaterals, quads.size());
            for (const auto& quad : quads) {
                GmfSetLin(meshb_idx, GmfQuadrilaterals, quad[0], quad[1], quad[2], quad[3], 0);
            }
            spdlog::info("Wrote {} quadrilaterals", quads.size());
        }

        // Separate solids by type for writing
        std::vector<std::vector<int>> tetrahedra;
        std::vector<std::vector<int>> pyramids;
        std::vector<std::vector<int>> prisms;
        std::vector<std::vector<int>> hexahedra;

        for (size_t i = 0; i + 1 < mesh_data.solid_vertices_offset_.size(); ++i) {
            Index start = mesh_data.solid_vertices_offset_[i];
            Index end = mesh_data.solid_vertices_offset_[i + 1];
            unsigned char type = mesh_data.solid_types_[i];

            std::vector<int> vertices;
            for (Index j = start; j < end; ++j) {
                vertices.push_back(mesh_data.solid_vertices_[j] + 1); // Convert to 1-based
            }

            switch (type) {
            case VTK_TETRA:
                if (vertices.size() == 4)
                    tetrahedra.push_back(vertices);
                break;
            case VTK_PYRAMID:
                if (vertices.size() == 5)
                    pyramids.push_back(vertices);
                break;
            case VTK_WEDGE:
                if (vertices.size() == 6)
                    prisms.push_back(vertices);
                break;
            case VTK_HEXAHEDRON:
                if (vertices.size() == 8)
                    hexahedra.push_back(vertices);
                break;
            default:
                spdlog::warn("Unsupported solid type {} at index {}", type, i);
                break;
            }
        }

        // Write Tetrahedra
        if (!tetrahedra.empty()) {
            GmfSetKwd(meshb_idx, GmfTetrahedra, tetrahedra.size());
            for (const auto& tet : tetrahedra) {
                GmfSetLin(meshb_idx, GmfTetrahedra, tet[0], tet[1], tet[2], tet[3], 0);
            }
            spdlog::info("Wrote {} tetrahedra", tetrahedra.size());
        }

        // Write Pyramids
        if (!pyramids.empty()) {
            GmfSetKwd(meshb_idx, GmfPyramids, pyramids.size());
            for (const auto& pyr : pyramids) {
                GmfSetLin(meshb_idx, GmfPyramids, pyr[0], pyr[1], pyr[2], pyr[3], pyr[4], 0);
            }
            spdlog::info("Wrote {} pyramids", pyramids.size());
        }

        // Write Prisms
        if (!prisms.empty()) {
            GmfSetKwd(meshb_idx, GmfPrisms, prisms.size());
            for (const auto& prism : prisms) {
                GmfSetLin(meshb_idx, GmfPrisms, prism[0], prism[1], prism[2],
                    prism[3], prism[4], prism[5], 0);
            }
            spdlog::info("Wrote {} prisms", prisms.size());
        }

        // Write Hexahedra
        if (!hexahedra.empty()) {
            GmfSetKwd(meshb_idx, GmfHexahedra, hexahedra.size());
            for (const auto& hex : hexahedra) {
                GmfSetLin(meshb_idx, GmfHexahedra, hex[0], hex[1], hex[2], hex[3],
                    hex[4], hex[5], hex[6], hex[7], 0);
            }
            spdlog::info("Wrote {} hexahedra", hexahedra.size());
        }

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Error writing mesh data: {}", e.what());
        return false;
    }
}