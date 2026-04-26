#ifndef MESH_ACTOR_SELECT_OP_H
#define MESH_ACTOR_SELECT_OP_H
#include <memory>
#include <optional>
#include <vtkProp.h>
#include <vtkPolyData.h>
#include <vtkExtractSelection.h>
#include <vtkIdTypeArray.h>

class MeshActorSelectOp;
class MeshActor;

/**
 * @brief MeshActor在选择过程中的操作句柄类，可以在选择系统中保存该句柄，延迟获取操作对象
 */
class MeshActorSelectOpFactory {
    friend MeshActorSelectOp;

public:
    MeshActorSelectOpFactory();
    MeshActorSelectOpFactory(std::weak_ptr<const MeshActor> mesh_actor);
    /**
     * @brief 获取操作对象
     * @return 如果MeshActor还存在，返回MeshActorSelectOp对象，否则返回std::nullopt
     */
    std::optional<MeshActorSelectOp> lock();

private:
    std::weak_ptr<const MeshActor> mesh_actor_;
};

/**
 * @brief MeshActor在选择过程中的操作类，作为临时对象使用
 */
class MeshActorSelectOp {
    friend MeshActorSelectOpFactory;

public:
    MeshActorSelectOp(std::shared_ptr<const MeshActor> mesh_actor);

    vtkProp& getSolidActor();
    vtkProp& getFaceActor();
    vtkProp& getEdgeActor();
    //vtkProp& getVertexActor();
    vtkProp& getBlockActor();

    /**
     * @brief 根据体id，提取体数据
     * @param[in] ids 体id数组
     * @return 提取出的体数据 Filter
     */
    vtkSmartPointer<vtkExtractSelection> extractSolid(vtkIdTypeArray* ids);

    /**
     * @brief 根据点id，提取点数据
     * @param ids 点id数组
     * @return 提取出的点数据 Filter
     */
    vtkSmartPointer<vtkExtractSelection> extractVertex(vtkIdTypeArray* ids);

     /**
     * @brief 根据边的点id对，提取边数据
     * @param ids 每个元素表示一条边（起始点ID，结束点ID）
     * @return 提取出的边数据 Filter
     */
    vtkSmartPointer<vtkPolyData> extractEdge(std::vector<std::array<vtkIdType, 2>>& ids);

private:
    std::shared_ptr<const MeshActor> mesh_actor_; // 非空
};
#endif // MESH_ACTOR_SELECT_OP_H