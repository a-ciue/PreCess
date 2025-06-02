#pragma once

#include <optional>            // 一定要包含 <optional>，否则找不到 std::optional
#include <QUrl>
#include "ICommand.h"
#include "../ModelImporter.h"  // 根据你的目录结构，这里假设 ModelImporter.h 在上一级

/**
 * ImportModelCommand: 负责将导入操作封装为 ICommand，
 * 在 execute() 时调用 ModelImporter::import(...)。
 * undo() 目前留空，后续需要撤销功能再补充。
 */
class ImportModelCommand : public ICommand {
public:
    // 构造函数：传入 ModelImporter 的引用，以及待导入文件的 URL
    explicit ImportModelCommand(ModelImporter& importer, const QUrl& path)
        : importer_(importer), path_(path) {}

    // 如果需要在前端拿到新建的 ModelOperator，可通过 result() 访问
    std::optional<ModelOperator> result() const { return op_; }

    // ICommand 接口：执行导入
    void execute() override;

    // ICommand 接口：撤销（暂不支持）
    void undo() override;

    void redo() override;         

private:
    ModelImporter& importer_;
    QUrl                                   path_;
    std::optional<ModelOperator>           op_;  // 保存导入得到的 ModelOperator
};
