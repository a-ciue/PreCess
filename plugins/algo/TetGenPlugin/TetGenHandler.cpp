#include "TetGenHandler.h"
#include "ModelIOSystemBase.h"
#include "ModelOperatorBase.h"
#include "TempFile.h"

#include <spdlog/spdlog.h>
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;
using fs::path;

std::any systems::algo::TetGenHandler::execute(HandlerContext& context, const std::vector<std::any>& args)
{
    // parse args
    if (args.size() < 3) {
        spdlog::critical("TetGenHandler: Not enough arguments provided.");
        return {};
    }
    const path* cmd = std::any_cast<path>(&args[0]);
    const int* keep_outer_idx = std::any_cast<int>(&args[1]);
    const double* scale = std::any_cast<double>(&args[2]);
    if (!cmd || !keep_outer_idx || *keep_outer_idx < 0 || !scale) {
        spdlog::critical("TetGenHandler: Invalid command or arguments.");
        return {};
    }
    bool keep_outer = *keep_outer_idx == 0; // 0: 是, 1: 否

    if (context.cur_model.getType() != ModelData::Type::Mesh) {
        spdlog::error("TetGenHandler: Current model is not a mesh, which is not supported by TetGen.");
        return {};
    }

    // 拼接命令行参数
    path temp_file = core::TempFile::instance().path();
    temp_file.replace_extension(temp_file.extension().string() + ".mesh");
    context.io_system.write(context.cur_model.getId(), temp_file, "Gamma Mesh Format @Medit", {});

    std::string full_command { "\"" + cmd->string() + "\" -pqG" + to_string(*scale) + "BNEFVYAg " + (keep_outer ? "-H ":" ") + temp_file.string() };
    spdlog::debug("TetGenHandler: Executing command: {}", full_command);
    std::system(full_command.c_str());

    path& output_file = temp_file.replace_extension(".1.mesh");
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
