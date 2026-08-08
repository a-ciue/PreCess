#include "TetGenHandler.h"
#include "ArgObject.h"
#include "ModelIOSystemBase.h"
#include "ComponentOperator.h"
#include "ComponentData.h"
#include "ModelLayer.h"
#include "TempFile.h"

#include <filesystem>
#include <spdlog/spdlog.h>

using namespace std;
using namespace core;
namespace fs = std::filesystem;
using fs::path;

std::any systems::algo::TetGenHandler::execute(HandlerContext& context, const std::vector<ArgObject>& args)
{
    // parse args
    if (args.size() < 3) {
        spdlog::critical("TetGenHandler: Not enough arguments provided.");
        return {};
    }
    const path* cmd = args[0].get<ArgTypeEnum::Path>();
    const int* keep_outer_idx = args[1].get<ArgTypeEnum::Combo>();
    const double* scale = args[2].get<ArgTypeEnum::Float>();
    if (!cmd || !keep_outer_idx || *keep_outer_idx < 0 || !scale) {
        spdlog::critical("TetGenHandler: Invalid command or arguments.");
        return {};
    }
    bool keep_outer = *keep_outer_idx == 0; // 0: 是, 1: 否

    // component 粒度：必须有 mesh
    const ComponentData& comp = context.cur_component.component();
    if (!comp.mesh) {
        spdlog::error("TetGenHandler: current component {} has no mesh, not supported by TetGen.",
            context.cur_component.componentId());
        return {};
    }

    // 写输入文件：只导出当前 component
    path temp_file = core::TempFile::instance().path();
    temp_file.replace_extension(temp_file.extension().string() + ".mesh");

    context.io_system.writeComponents({ context.cur_component.componentId() },
        temp_file,
        "Gamma Mesh Format @Medit",
        {});

    // 调 tetgen
    std::string full_command { "\"" + cmd->string() + "\" -pqG" + to_string(*scale) + "BNEFVYAg " + (keep_outer ? "-H " : " ") + temp_file.string() };
    spdlog::debug("TetGenHandler: Executing command: {}", full_command);
    std::system(full_command.c_str());

    // 读输出（read 仍是导入模型，不依赖 component）
    path output_file = temp_file;
    output_file.replace_extension(".1.mesh");

    if (fs::exists(output_file)) {
        context.io_system.read(output_file, "Gamma Mesh Format @Medit", {});
    } else {
        spdlog::critical("TetGenHandler: output file not found in {}", output_file.string());
    }
    return {};
}

std::vector<ArgType> systems::algo::TetGenHandler::args_type() const
{
    // 复用TetGenCommand参数模型
    return {
        ArgType { ArgTypeEnum::Path, "TetGen路径", "tetgen" },
        ArgType { ArgTypeEnum::Combo, "是否仅保留最外层腔体", "是,否" },
        ArgType { ArgTypeEnum::Float, "尺度参数（比1越大越稀疏）", "1.2" },
    };
}
