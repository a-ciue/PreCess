/**
 * @file StepModelHandler.h
 * @author 张家僮(htxz_6a6@163.com)
 */
#ifndef M_MODEL_HANDLER_H
#define M_MODEL_HANDLER_H
#include "ModelIOHandler.h"

class ModelData;
class CTMeshModel;

namespace systems::io {
/**
 * @brief OBJ模型文件处理器
 */
class StepModelHandler : public ModelIOHandler {
public:
    StepModelHandler() = default;
    ~StepModelHandler() override = default;

   std::unique_ptr<ModelData> read_model(const fs::path& path, const std::vector<std::any>& args) override;
    void write_components(const ModelManager& mgr,
        const std::vector<Index>& component_ids,
        const fs::path& path,
        const std::vector<std::any>& args) override;
    std::vector<core::ArgType> read_args_type() const override;
    std::vector<core::ArgType> write_args_type() const override;
};

}
#endif // !M_MODEL_HANDLER_H 