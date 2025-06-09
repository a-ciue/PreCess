//
// Created by 张家僮 on 5/14/25.
//
// command/SplitEdgeCommand.cpp
#include "SplitEdgeCommand.h"
#include <QVariantList>

SplitEdgeCommand::SplitEdgeCommand(ModelOperator model_op, QSelection* selection)
        : model_op_(model_op), selection_(selection) { }

void SplitEdgeCommand::execute() {
    model_op_.split_edge(selection_);
}

void SplitEdgeCommand::undo() {
    // TODO: 实现撤销逻辑（例如删除新顶点、恢复原面结构）
}

void SplitEdgeCommand::redo()
{
}

QList<ArgTypeObject*> SplitEdgeCommand::getArgsModel()
{
    QList<ArgTypeObject*> model;
    // 添加一个面选择器
    model.append(new ArgTypeObject(3, "选择边", "无"));

    return model;
}

unique_ptr<SplitEdgeCommand> SplitEdgeCommand::create(ModelOperator model_op, ModelImporter& importer, const QVariantList& list)
{
    // 根据传入的参数创建 SplitFaceCommand 对象
    return std::make_unique<SplitEdgeCommand>(model_op, list.at(0).value<QSelection*>());
}
