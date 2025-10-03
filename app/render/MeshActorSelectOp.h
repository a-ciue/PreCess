#ifndef MESH_ACTOR_SELECT_OP_H
#define MESH_ACTOR_SELECT_OP_H
#include "Core.h"

#include <vtkPropCollection.h>
#include <memory>

class MeshActor;
class MeshActorSelectOp {
public:
    MeshActorSelectOp();
    MeshActorSelectOp(std::weak_ptr<const MeshActor> mesh_actor);

    /**
     * @brief 将actor添加到pick_list中
     * @param pick_list actor的集合，用于选择
     * @return true 成功添加,false 添加失败(可能是mesh_actor已经被释放)
     */
    bool addPickList(vtkPropCollection* pick_list);

    Index getModelBlockId(vtkIdType block_id);

    vtkIdType getSolidIdByFace(vtkIdType face_id);

private:
    std::weak_ptr<const MeshActor> mesh_actor_;
};
#endif // MESH_ACTOR_SELECT_OP_H