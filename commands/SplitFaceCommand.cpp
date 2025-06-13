//
// Created by 徐昊阳 on 4/12/25.
//
// command/SplitFaceCommand.cpp
#include "SplitFaceCommand.h"
#include <QVariantList>

SplitFaceCommand::SplitFaceCommand(ModelOperator model_op, QSelection* selection)
        : model_op_(model_op), selection_(selection) { }

void SplitFaceCommand::execute() {
    //// 从 selection 中取出要拆分的面 ID
    //auto sel = selection_->move();
    //if (!sel || sel->ids.empty()) {
    //    throw std::invalid_argument("未选择任何面进行拆分");
    //}
    //int face_id = sel->ids[0];
    //// 找到对应的 patch
    //auto* meshPtr = const_cast<MeshLib::CTMesh*>(model_->mesh());
    //int patch_id = meshPtr->idFace(face_id)->get_g();
    //auto* face = meshPtr->idFace(face_id);
    //// 计算面的质心坐标
    //CPoint mid;
    //int count = 0;
    //for (MeshLib::CTMesh::FaceVertexIterator vi(face); !vi.end(); ++vi) {
    //    mid += vi.value()->point();
    //    ++count;
    //}
    //mid /= count;
    //// 记录原父 ID，以保留层级关系
    //int father_id = model_->patches().at(patch_id)->father_id;
    //// 调用工具函数插入新顶点并拆分面
    //auto* newVertex = ModelUtil::split_face(face, meshPtr);
    //newVertex->point() = mid;
    //// 恢复父 ID
    //model_->update_father_id(patch_id, father_id);
    //// 更新 patch 和 actor（会发出 patchUpdated 等信号）
    //model_->update_patches(std::vector<int>{patch_id}, false);
    //model_->update_actors({patch_id});

    model_op_.split_face(selection_);
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
