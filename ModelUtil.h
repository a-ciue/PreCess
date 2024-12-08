#ifndef MODELUTIL_H
#define MODELUTIL_H
#include <filesystem>
#include <memory>
#include <vector>
#include <vtkNew.h>

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

    //! @brief 三分三角形，返回中间添加的点，注意维护m_g在内的新面属性
    //! @param face 待切分的三角形
    //! @param mesh 三角形所在的网格
    //! @return 新添加的点，可以调整该点的坐标
    static MeshLib::CToolVertex* split_face(MeshLib::CToolFace* face, MeshLib::CTMesh* mesh);
    //! @brief 切分边，两侧在切点处对邻接三角形做二分。注意维护m_g在内的面属性
    //! @param edge 待切分的边
    //! @param mesh 边所在网格
    //! @return 新添加的点，可以调整该点坐标
    static MeshLib::CToolVertex* split_edge(MeshLib::CToolEdge* edge, MeshLib::CTMesh* mesh);

    static vtkNew<vtkMinimalStandardRandomSequence> randomSequence;
    static vtkNew<vtkNamedColors> colors;

private:
    static std::string cmdPopen(const std::string& cmdLine);
    static void _attach_halfedge_to_edge(MeshLib::CToolHalfEdge* he0, MeshLib::CToolHalfEdge* he1, MeshLib::CToolEdge* e);
	static void RemoveReverse(std::string filename);
};
#endif // MODELUTIL_H
