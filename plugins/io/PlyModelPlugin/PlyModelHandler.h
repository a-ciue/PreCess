/**
 * @file PlyModelHandler.h
 * @author 龚正(1740124400@qq.com)
 * 支持ASCII和二进制格式的PLY文件
 */
#ifndef PLY_MODEL_HANDLER_H
#define PLY_MODEL_HANDLER_H
#include "ModelIOHandler.h"
#include <tinyply.h>
class ModelData;

namespace systems::io {
/**
 * @brief PLY模型文件处理器
 */
class PlyModelHandler : public ModelIOHandler {
public:
    PlyModelHandler() = default;
    ~PlyModelHandler() override = default;

    std::optional<ModelPayload> read_model(const fs::path& path, const std::vector<std::any>& args) override;
    void write_components(const ModelLayer& mgr,
        const std::vector<Index>& component_ids,
        const fs::path& path,
        const std::vector<std::any>& args) override;
    std::vector<core::ArgType> read_args_type() const override;
    std::vector<core::ArgType> write_args_type() const override;
};

}
#endif // !PLY_MODEL_HANDLER_H 