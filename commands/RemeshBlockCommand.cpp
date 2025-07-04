//
// Created by 张家僮 on 5/14/25.
//
// command/RemeshBlockCommand.cpp
#include "RemeshBlockCommand.h"
#include <QVariantList>

RemeshBlockCommand::RemeshBlockCommand(ModelOperator model_op, QSelection* selection)
        : model_op_(model_op), selection_(selection) { }

void RemeshBlockCommand::execute() {
    auto* md = asMeshData();
    auto sel = selection->move();
    const std::vector<int>& block_ids = sel->ids;
    // 验证 block_id 是否有效
    std::unordered_set<int> patch_ids_set;
    for (int block_id : block_ids) {
        if (md->blocks_.find(block_id) == md->blocks_.end()) {
            throw std::runtime_error("Block ID not found in group: " + std::to_string(block_id));
        }
        auto& block = md->blocks_[block_id];
        patch_ids_set.insert(block->patchIDs.begin(), block->patchIDs.end());
    }

    // 将 patch_ids_set 转换为 vector
    std::vector<int> patch_ids(patch_ids_set.begin(), patch_ids_set.end());

    // 调用 ModelUtil::remesh_patches 方法对 patch 重新网格化
    md->mesh_ = ModelUtil::remesh_patches(std::move(md->mesh_), patch_ids);

    // 更新所有相关的 patches
    //update_patches(patch_ids, false);

    // 更新所有patches
    std::vector<Index> all_patch_ids;
    for (auto& face : md->mesh_->faces()) {
        all_patch_ids.push_back(face->get_g());
    }

    // 更新 patches_
    md->update_patches(all_patch_ids, false);

    model_op_.notifyChanged();
}

void RemeshBlockCommand::undo() {
    // TODO: 实现撤销逻辑（例如删除新顶点、恢复原面结构）
}

void RemeshBlockCommand::redo()
{
}

QList<ArgTypeObject*> RemeshBlockCommand::getArgsModel()
{
    QList<ArgTypeObject*> model;
    // 添加一个面选择器
    model.append(new ArgTypeObject(3, "选择块", "无"));

    return model;
}

unique_ptr<RemeshBlockCommand> RemeshBlockCommand::create(ModelOperator model_op, ModelImporter& importer, const QVariantList& list)
{
    // 根据传入的参数创建 SplitFaceCommand 对象
    return std::make_unique<RemeshBlockCommand>(model_op, list.at(0).value<QSelection*>());
}
