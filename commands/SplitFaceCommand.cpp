//
// Created by 徐昊阳 on 4/12/25.
//
// command/SplitFaceCommand.cpp
#include "SplitFaceCommand.h"
#include <QVariantList>

SplitFaceCommand::SplitFaceCommand(ModelOperator model_op, QSelection* selection)
        : model_op_(model_op), selection_(selection) { }

void SplitFaceCommand::execute() {
    auto* md = asMeshData();
    // 从 selection 中取出 Selection 对象
    auto sel = selection->move();
    // 假定 sel->ids[0] 为 patch_id，sel->ids[1] 为 face_id
    int face_id = sel->ids[0];
    int patch_id = md->mesh_->idFace(face_id)->get_g();
    //int face_gid = patches[patch_id]->faceIDs_[face_id];
    int face_gid = face_id;
    MeshLib::CToolFace* face = md->mesh_->idFace(face_gid);

    CPoint mid;
    int i = 0;
    for (MeshLib::CTMesh::FaceVertexIterator vi(face); !vi.end(); vi++)
    {
        mid += vi.value()->point();
        ++i;
    }
    mid /= i;

    std::vector<int> affected_patch_ids = { patch_id };
    // 执行面切分操作
    ModelUtil::split_face(face, md->mesh_.get())->point() = mid;

    md->update_patches(std::vector{ patch_id }, false);

    model_op_.notifyChanged();
}

void SplitFaceCommand::undo() {
    // TODO: 实现撤销逻辑（例如删除新顶点、恢复原面结构）
}

void SplitFaceCommand::redo()
{
}

QList<ArgTypeObject*> SplitFaceCommand::getArgsModel()
{
    QList<ArgTypeObject*> model;
    // 添加一个面选择器
    model.append(new ArgTypeObject(3, "选择面", "无"));

    return model;
}

unique_ptr<SplitFaceCommand> SplitFaceCommand::create(ModelOperator model_op, ModelImporter& importer, const QVariantList& list)
{
    // 根据传入的参数创建 SplitFaceCommand 对象
    return std::make_unique<SplitFaceCommand>(model_op, list.at(0).value<QSelection*>());

}
