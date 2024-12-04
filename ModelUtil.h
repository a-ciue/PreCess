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
    //! @brief 函数读取样条文件，调用网格剖分算法，返回剖分得到的网格
    //! @param spline_dir 样条文件路径
    //! @return 网格对象
    static std::unique_ptr<MeshLib::CTMesh> mesh_from_spline(std::filesystem::path spline_dir);
    //! @brief 对给定网格对象和指定patch进行重网格，需要调用重网格和拼接两个功能
    //! @param mesh 被重网格的网格对象
    //! @param patch_ids 需要重网格的patch
    //! @return 重网格完毕后拼接完毕的完整网格
    static std::unique_ptr<MeshLib::CTMesh> remesh_patches(std::unique_ptr<MeshLib::CTMesh> mesh, const std::vector<int>& patch_ids);

    static vtkMinimalStandardRandomSequence randomSequence;
    static vtkNamedColors colors;

private:
    static std::string cmdPopen(const std::string& cmdLine);
};
