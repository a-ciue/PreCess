/**
 * @file MeshMeditModelHandler.h
 * @author 张家僮(htxz_6a6@163.com)
 */
#ifndef MESH_MEDIT_MODEL_HANDLER_H
#define MESH_MEDIT_MODEL_HANDLER_H
#include "ModelIOHandler.h"

class ModelData;
class CTMeshModel;

namespace systems::io {
/**
 * @brief .mesh 模型文件处理器
 */
class MeshMeditModelHandler : public ModelIOHandler {
public:
    MeshMeditModelHandler() = default;
    ~MeshMeditModelHandler() override = default;

    /**
     * @brief 读取模型文件，返回 ModelData 对象
     *
     * TODO: 由于 libmeshb7 库的实现要求，目前 path 必须是以 .mesh/.meshb 等特定拓展名结尾，以后可以考虑通过参数传递该信息，结合临时文件构造带对应拓展名文件给库作为输入。
     *
     * @param path 读入文件路径。
     * @param args 参数
     * @return 读取到的模型对象
     */
    std::optional<ModelPayload> read_model(const fs::path& path, const std::vector<std::any>& args) override;
    void write_components(const ModelLayer& mgr,
        const std::vector<Index>& component_ids,
        const fs::path& path,
        const std::vector<std::any>& args) override;
    std::vector<core::ArgType> read_args_type() const override;
    std::vector<core::ArgType> write_args_type() const override;
};

}
#endif // !MESH_MEDIT_MODEL_HANDLER_H 