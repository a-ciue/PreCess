/**
 * @file VtkXmlModelHandler.h
 * @brief VTK XML 网格文件处理器
 */
#ifndef VTK_XML_MODEL_HANDLER_H
#define VTK_XML_MODEL_HANDLER_H
#include "ModelIOHandler.h"

class ModelData;

namespace systems::io {
/**
 * @brief VTK XML 网格格式处理器，读取和写出 vtp / vtu 文件
 */
class VtkXmlModelHandler : public ModelIOHandler {
public:
    VtkXmlModelHandler() = default;
    ~VtkXmlModelHandler() override = default;

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
#endif // !VTK_XML_MODEL_HANDLER_H
