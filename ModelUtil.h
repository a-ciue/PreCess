#include <filesystem>
#include <memory>
#include <vector>
#include <stdio.h>
#include <stdlib.h>

class vtkNamedColors;

namespace MeshLib {
template <typename V, typename E, typename F, typename H>
class CToolMesh;
class CToolVertex;
class CToolEdge;
class CToolFace;
class CToolHalfEdge;
typedef CToolMesh<CToolVertex, CToolEdge, CToolFace, CToolHalfEdge> CTMesh;
}
class vtkMinimalStandardRandomSequence;

class ModelUtil {
public:
    static std::unique_ptr<MeshLib::CTMesh> mesh_from_spline(std::filesystem::path spline_dir);
    static std::unique_ptr<MeshLib::CTMesh> remesh_patches(std::unique_ptr<MeshLib::CTMesh> mesh, const std::vector<int>& patch_ids);

    static vtkMinimalStandardRandomSequence randomSequence;
    static vtkNamedColors colors;

private:
    std::string cmdPopen(const std::string& cmdLine);
};
