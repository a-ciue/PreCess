/**
 * @file GmshModelHandler.h
 * @brief Gmsh 网格文件处理器
 */
#ifndef GMSH_MODEL_HANDLER_H
#define GMSH_MODEL_HANDLER_H
#include "ModelIOHandler.h"

class ModelData;

namespace systems::io {
/**
 * @brief Gmsh 网格格式处理器，读取和写出 msh 文件
 */
class GmshModelHandler : public ModelIOHandler {
public:
    GmshModelHandler() = default;
    ~GmshModelHandler() override = default;

    std::optional<ModelPayload> read_model(const fs::path& path,
        const std::vector<std::any>& args) override;
    void write_components(const ModelLayer& mgr,
        const std::vector<Index>& component_ids,
        const fs::path& path,
        const std::vector<std::any>& args) override;
    std::vector<core::ArgType> read_args_type() const override;
    std::vector<core::ArgType> write_args_type() const override;
};

}
#endif // !GMSH_MODEL_HANDLER_H
