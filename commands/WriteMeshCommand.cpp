#include "ImportModelCommand.h"

// 定义 execute，注意这里 **不带 override**，只写函数体
void ImportModelCommand::execute() {
    //! @param mesh_path 输出文件路径
    //! @param mode 选定输出模式
    //! @param extension 输出文件拓展名
    std::function<int(int)> gid{};
    auto* md = asMeshData();

    if (!md)
    {
        std::cerr << "call write mesh but not a mesh\n";
        return;
    }

    switch (mode) {
    case ModelRenderMode::Face:
        {
        gid = [](int patch_id) {
            return 1;
        };
        break;
    }
    case ModelRenderMode::Block: {
        auto* md = this->asMeshData();
        gid = [md](int patch_id) {
            //const auto& patch = md->patches_.at(patch_id);
            return md->blocks_[md->patches_[patch_id]->blockID]->id;
        };
        break;
    }
    }

    if (extension == "obj")
        ModelUtil::write_group_obj(md->mesh_.get(), mesh_path, gid);
    else if (extension == "inp")
        ModelUtil::write_group_inp(md->mesh_.get(), mesh_path, gid);
    else
        //"不支持的文件类型"
        assert(false);
}

// 空实现
void ImportModelCommand::undo() {
    // 如果未来需要撤销，可以在这里调用 ModelManager::removeModel(...)
}

void ImportModelCommand::redo() {
    execute();
}
