/**
 * @file StlModelHandler.h
 * @brief STL 三角网格文件处理器
 */
#ifndef STL_MODEL_HANDLER_H
#define STL_MODEL_HANDLER_H
#include "ModelIOHandler.h"

class ModelData;

namespace systems::io {
/**
 * @brief STL 三角网格格式处理器，读取和写出 STL 文件
 */
class StlModelHandler : public ModelIOHandler {
public:
    StlModelHandler() = default;
    ~StlModelHandler() override = default;

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
#endif // !STL_MODEL_HANDLER_H
