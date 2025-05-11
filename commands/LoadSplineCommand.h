//
// Created by 徐昊阳 on 4/12/25.
//
// command/LoadSplineCommand.h
#pragma once
#include "ICommand.h"
#include <QUrl>
#include <QString>
class ModelManager;

/**
 * LoadSplineCommand：加载样条文件并创建模型的命令
 */
class LoadSplineCommand : public ICommand {
public:
    LoadSplineCommand(ModelManager* mgr, const QUrl& path);
    void execute() override;
    void undo() override;
private:
    ModelManager* manager_;
    QUrl splinePath_;
    QString modelName_;
};
