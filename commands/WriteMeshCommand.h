#pragma once

#include <optional>            // 一定要包含 <optional>，否则找不到 std::optional
#include <QUrl>
#include "ICommand.h"
#include "../ModelImporter.h"  // 根据你的目录结构，这里假设 ModelImporter.h 在上一级
#include <filesystem>

/**
 * WriteMeshCommand: 负责将写网格操作封装为 ICommand，
 * 输出网格文件，选择面输出（不带组信息）、块输出、组输出
 * undo() 目前留空，后续需要撤销功能再补充。
 */
class WriteMeshCommand : public ICommand {
public:
    // 构造函数：待写入文件的 URL
    explicit WriteMeshCommand(ModelOperator model_op, const QUrl& path)
        : path_(path.path().toStdU32String()) {}

    // ICommand 接口：执行导入
    void execute() override;

    // ICommand 接口：撤销（暂不支持）
    void undo() override;

    void redo() override;         

private:
    std::filesystem::path                                   path_;
    std::optional<ModelOperator>           op_;  // 保存导入得到的 ModelOperator
};
