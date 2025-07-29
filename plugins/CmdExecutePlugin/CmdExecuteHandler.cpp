#include "CmdExecuteHandler.h"
#include "ModelIOSystemBase.h"
#include "ModelOperatorBase.h"
#include <spdlog/spdlog.h>
#include <filesystem>

std::any systems::algo::CmdExecuteHandler::execute(HandlerContext& context, const std::vector<std::any>& args)
{
    using namespace std;
    using namespace std::filesystem;
    if (args.size() < 2)
        return {};
    const path* cmd = std::any_cast<path>(&args[0]);
    const string* args_str = std::any_cast<string>(&args[1]);
    if (!cmd || !args_str) {
        spdlog::critical("CmdExecuteHandler: Invalid command or arguments.");
        return {};
    }

    path exe_dir = *cmd;
    exe_dir.remove_filename();

    string input_file {};
    if (context.cur_model.getType() == ModelData::Type::Mesh) {
        input_file = "input.obj";
        // model_op_.write_mesh(exe_dir / input_file, ModelRenderMode::Face, "obj");
        context.io_system.write(context.cur_model.getId(), exe_dir / input_file, "Wavefront .obj file", {});
    } else {
        input_file = "input.stp";
        // model_op_.write_spline(exe_dir / input_file);
        context.io_system.write(context.cur_model.getId(), exe_dir / input_file, "ISO 10303-21", {});
    }

    std::string full_command { cmd->string() + " " + input_file + " " + *args_str };
    spdlog::debug("CmdExecuteHandler: Executing command: {}", full_command);
    std::system(full_command.c_str());

    path output_dir;
    if (std::filesystem::exists(output_dir = exe_dir / "output.obj")) {
        context.io_system.read(output_dir, "Wavefront .obj file", {});
    } else if (std::filesystem::exists(output_dir = exe_dir / "output.stp")) {
        context.io_system.read(output_dir, "ISO 10303-21", {});
    } else {
        spdlog::critical("CmdExecuteHandler: output file not found in {}", exe_dir.string());
    }
    return {};
}

std::vector<ArgType> systems::algo::CmdExecuteHandler::args_type() const
{
    // 复用CmdExecuteCommand参数模型
    return {
        ArgType { ArgTypeEnum::Path, "功能(.exe,.bat)", "" },
        ArgType { ArgTypeEnum::Text, "参数列表", "" }
    };
}
