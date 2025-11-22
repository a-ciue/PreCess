/**
 * @file VtkLegacyModelHandler.h
 * @author 张家僮(htxz_6a6@163.com)
 */
#ifndef VTK_LEGACY_MODEL_HANDLER_H
#define VTK_LEGACY_MODEL_HANDLER_H
#include "ModelIOHandler.h"

class ModelData;
class CTMeshModel;

namespace systems::io {
/**
 * @brief VTK模型文件处理器
 */
class VtkLegacyModelHandler : public ModelIOHandler {
public:
    VtkLegacyModelHandler() = default;
    ~VtkLegacyModelHandler() override = default;

    std::unique_ptr<ModelData> read_model(const fs::path& path, const std::vector<std::any>& args) override;
    void write_model(const ModelData& data, const fs::path& path, const std::vector<std::any>& args) override;
    std::vector<core::ArgType> read_args_type() const override;
    std::vector<core::ArgType> write_args_type() const override;
};

}
#endif // !VTK_LEGACY_MODEL_HANDLER_H 