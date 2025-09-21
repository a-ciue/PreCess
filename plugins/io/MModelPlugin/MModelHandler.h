/**
 * @file MModelHandler.h
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
class MModelHandler : public ModelIOHandler {
public:
    MModelHandler() = default;
    ~MModelHandler() override = default;

    std::unique_ptr<ModelData> read_model(const fs::path& path, const std::vector<std::any>& args) override;
    void write_model(const ModelData& data, const fs::path& path, const std::vector<std::any>& args) override;
    std::vector<ArgType> read_args_type() const override;
    std::vector<ArgType> write_args_type() const override;
};

}
#endif // !M_MODEL_HANDLER_H 