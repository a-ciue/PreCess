//
// Created by 徐昊阳 on 4/12/25.
//
// command/LoadSplineCommand.cpp
#include "LoadSplineCommand.h"
#include "ModelManager.h"

LoadSplineCommand::LoadSplineCommand(ModelManager* mgr, const QUrl& path)
        : manager_(mgr), splinePath_(path) {
    modelName_ = path.fileName();
}

void LoadSplineCommand::execute() {
    manager_->readSpline(splinePath_);
}

void LoadSplineCommand::undo() {
    manager_->removeModel(modelName_);
}
