//
// Created by 徐昊阳 on 4/12/25.
//
// command/MergeBlocksCommand.cpp
#include "MergeBlocksCommand.h"
#include "../QSelection.h"
#include <QVariantList>

MergeBlocksCommand::MergeBlocksCommand(ModelOperator model_op, QSelection* selection)
    : model_op_(model_op)
    , selection_(selection) { }

void MergeBlocksCommand::execute() {
    //auto sel = selection_->move();
    //const auto& block_ids = sel->ids;
    //if (block_ids.empty()) {
    //    throw std::invalid_argument("未提供要合并的块 ID 列表");
    //}
    //int target_id = block_ids[0];
    //if (!model_->blocks.count(target_id)) {
    //    throw std::runtime_error("目标块不存在: " + std::to_string(target_id));
    //}
    //auto& target_block = model_->blocks.at(target_id);
    //// 记录父 ID
    //int father_id = -1;
    //if (!target_block->patchIDs.empty()) {
    //    father_id = model_->patches.at(*target_block->patchIDs.begin())->father_id;
    //}
    //std::unordered_set<int> affectedGroups;
    //// 从第二个块开始合并
    //for (size_t i = 1; i < block_ids.size(); ++i) {
    //    int bid = block_ids[i];
    //    if (!model_->blocks.count(bid)) {
    //        throw std::runtime_error("块不存在: " + std::to_string(bid));
    //    }
    //    auto& blk = model_->blocks.at(bid);
    //    for (int pid : blk->patchIDs) {
    //        target_block->patchIDs.insert(pid);
    //        model_->patches.at(pid)->blockID = target_id;
    //        model_->update_father_id(pid, father_id);
    //    }
    //    affectedGroups.insert(blk->groupID);
    //    model_->groups_.at(blk->groupID)->blockIDs.erase(bid);
    //    if (model_->groups_.at(blk->groupID)->blockIDs.empty()) {
    //        model_->groups_.erase(blk->groupID);
    //    }
    //    model_->blocks.erase(bid);
    //}
    //// 发出合并完成信号
    //emit model_->blocksMerged(model_->getModelName(), block_ids, target_id, target_block->patchIDs);
    //// 发出分组更新信号
    //emit model_->groupUpdated(model_->getModelName(), target_block->groupID,
    //                          model_->groups_.at(target_block->groupID)->blockIDs);
    //for (int gid : affectedGroups) {
    //    if (model_->groups_.count(gid)) {
    //        emit model_->groupUpdated(model_->getModelName(), gid,
    //                                  model_->groups_.at(gid)->blockIDs);
    //    } else {
    //        emit model_->groupUpdated(model_->getModelName(), gid, {});
    //    }
    //}

    model_op_.merge_blocks(selection_);
}

void MergeBlocksCommand::undo() {
    // TODO: 实现撤销逻辑（将块拆分回原状态）
}

void MergeBlocksCommand::redo()
{
}

QList<QArgObject*> MergeBlocksCommand::getArgsModel()
{
    QList<QArgObject*> model;
    // 添加一个面选择器
    model.append(new QArgObject({ ArgTypeEnum::Selector, "选择块", "无", "" }));

    return model;
}

std::unique_ptr<MergeBlocksCommand> MergeBlocksCommand::create(ModelOperator model_op, ModelImporter& importer, const QVariantList& list)
{
    // 根据传入的参数创建 SplitFaceCommand 对象
    return std::make_unique<MergeBlocksCommand>(model_op, list.at(0).value<QSelection*>());
}
