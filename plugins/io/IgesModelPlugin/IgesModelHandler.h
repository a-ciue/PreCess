/**
 * @file IgesModelHandler.h
 * @brief IGES 模型文件处理器
 * @author 范成通
 */
#ifndef IGES_MODEL_HANDLER_H
#define IGES_MODEL_HANDLER_H
#include "ModelIOHandler.h"

class ModelData;

namespace systems::io {
/**
 * @brief IGES 文件格式处理器，读取和写入 IGES 文件
 */
class IgesModelHandler : public ModelIOHandler {
public:
    IgesModelHandler() = default;
    ~IgesModelHandler() override = default;

    std::unique_ptr<ModelData> read_model(const fs::path& path,
        const std::vector<std::any>& args) override;
    void write_components(const ModelLayer& mgr,
        const std::vector<Index>& component_ids,
        const fs::path& path,
        const std::vector<std::any>& args) override;
    std::vector<core::ArgType> read_args_type() const override;
    std::vector<core::ArgType> write_args_type() const override;
};

}
#endif // !IGES_MODEL_HANDLER_H