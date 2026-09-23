/**
 * @file QhexModelHandler.h
 * @brief Qhex 六面体网格文件处理器
 */
#ifndef QHEX_MODEL_HANDLER_H
#define QHEX_MODEL_HANDLER_H
#include "ModelIOHandler.h"

class ModelData;

namespace systems::io {
/**
 * @brief Qhex 实验室六面体网格格式处理器，读取和写出 Qhex 文件
 */
class QhexModelHandler : public ModelIOHandler {
public:
    QhexModelHandler() = default;
    ~QhexModelHandler() override = default;

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
#endif // !QHEX_MODEL_HANDLER_H
