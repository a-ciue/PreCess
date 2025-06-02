#include "ImportModelCommand.h"

// 定义 execute，注意这里 **不带 override**，只写函数体
void ImportModelCommand::execute() {
    // 调用 ModelImporter，将文件真正导入，返回一个 ModelOperator
    op_ = importer_.import(path_);
}

// 空实现
void ImportModelCommand::undo() {
    // 如果未来需要撤销，可以在这里调用 ModelManager::removeModel(...)
}

void ImportModelCommand::redo() {
    execute();
}
