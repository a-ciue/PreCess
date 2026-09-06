/**
 * @file OffModelHandler.h
 * @brief OFF 模型文件处理器
 */
#ifndef OFF_MODEL_HANDLER_H
#define OFF_MODEL_HANDLER_H
#include "ModelIOHandler.h"

class ModelData;

namespace systems::io {
/**
 * @brief OFF(Geomview Object File Format)文件格式处理器，读取和写出 OFF 文件
 */
class OffModelHandler : public ModelIOHandler {
public:
    OffModelHandler() = default;
    ~OffModelHandler() override = default;

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
#endif // !OFF_MODEL_HANDLER_H
