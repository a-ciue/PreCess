#ifndef OBJ_MODEL_HANDLER_H
#define OBJ_MODEL_HANDLER_H
#include "ModelIOHandler.h"

class ModelData;
class CTMeshModel;

namespace systems::io {
/**
 * @brief OBJ模型文件处理器
 */
class OBJModelHandler : public ModelIOHandler {
public:
    OBJModelHandler() = default;
    ~OBJModelHandler() override = default;

    std::unique_ptr<ModelData> read_model(const fs::path& path, const vector<std::any>& args) override;
    void write_model(const ModelData& data, const fs::path& path, const vector<std::any>& args) override;
    vector<ArgType> read_args_type() const override;
    vector<ArgType> write_args_type() const override;
    string file_type() const override;
    vector<string> file_extensions() const override;
};

}
#endif // !OBJ_MODEL_HANDLER_H 