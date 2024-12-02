#include <filesystem>
#include <memory>
#include <vector>

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
    static std::unique_ptr<MeshLib::CTMesh> remesh_patches(std::unique_ptr<MeshLib::CTMesh> mesh, std::vector<int> patch_ids);

private:
    static vtkMinimalStandardRandomSequence randomSequence;
}