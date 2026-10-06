/**
 * @file MModelHandler.h
 * @author 张家僮(htxz_6a6@163.com)
 */
#ifndef M_MODEL_HANDLER_H
#define M_MODEL_HANDLER_H
#include "ModelIOHandler.h"

class ModelData;

namespace systems::io {
/**
 * @brief M (.m) 网格文件处理器
 *
 * 数值属性支持 key=(v1 v2 ...)。无值标记名称须匹配 [A-Za-z_][A-Za-z0-9_]*，
 * 出现记为 1，缺失元素补 0，导出统一为数值属性形式。
 * 读取时先扫描顶点和面，再流式读取边；边单元及属性保持文件记录顺序。
 */
class MModelHandler : public ModelIOHandler {
public:
    MModelHandler() = default;
    ~MModelHandler() override = default;

    std::optional<ModelPayload> read_model(const fs::path& path, const std::vector<std::any>& args) override;
    void write_components(const ModelLayer& mgr,
        const std::vector<Index>& component_ids,
        const fs::path& path,
        const std::vector<std::any>& args) override;

    std::vector<core::ArgType> read_args_type() const override;
    std::vector<core::ArgType> write_args_type() const override;
};

}
#endif // !M_MODEL_HANDLER_H
