/**
 * @file MModelHandler.h
 * @author 张家僮(htxz_6a6@163.com)
 */
#ifndef M_MODEL_HANDLER_H
#define M_MODEL_HANDLER_H
#include "ModelIOHandler.h"

class ModelData;

namespace systems::io {
/**
 * @brief M (.m) 网格文件处理器
 */
class MModelHandler : public ModelIOHandler {
public:
    MModelHandler() = default;
    ~MModelHandler() override = default;

    std::optional<ModelPayload> read_model(const fs::path& path, const std::vector<std::any>& args) override;
    void write_components(const ModelLayer& mgr,
        const std::vector<Index>& component_ids,
        const fs::path& path,
        const std::vector<std::any>& args) override;

    std::vector<core::ArgType> read_args_type() const override;
    std::vector<core::ArgType> write_args_type() const override;
};

}
#endif // !M_MODEL_HANDLER_H 