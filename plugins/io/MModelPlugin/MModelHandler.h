/**
 * @file MModelHandler.h
 * @author 张家僮(htxz_6a6@163.com)
 */
#ifndef M_MODEL_HANDLER_H
#define M_MODEL_HANDLER_H
#include "ModelIOHandler.h"
#include <iosfwd>
#include <memory>

class ModelData;
struct MeshData;

namespace systems::io {
/**
 * @brief M (.m) 网格文件处理器
 *
 * 数值属性支持 key=(v1 v2 ...)。无值标记名称须匹配 [A-Za-z_][A-Za-z0-9_]*，
 * 出现记为 1，缺失元素补 0，导出统一为数值属性形式。
 * 读取时先扫描顶点和面，再流式读取边；边单元及属性保持文件记录顺序。
 * 写出时坐标及全部数值属性均保留 double 往返精度；相比旧版默认 6 位有效数字，
 * 文本可能变长，因此不保证与旧版导出文件逐字节一致。
 */
class MModelHandler : public ModelIOHandler {
public:
    MModelHandler() = default;
    ~MModelHandler() override = default;

    /**
     * @brief 打开文件并读取模型，非法记录按既有规则跳过
     * @param path 待读取的文件路径
     * @param args 读取参数，当前处理器不使用
     * @return 成功返回模型数据；打开、读取或装配失败时记录错误并返回 std::nullopt
     * @note 插件入口捕获内部标准异常及未知异常，不将读取异常传递到宿主 DLL 边界外。
     */
    std::optional<ModelPayload> read_model(const fs::path& path, const std::vector<std::any>& args) override;
    void write_components(const ModelLayer& mgr,
        const std::vector<Index>& component_ids,
        const fs::path& path,
        const std::vector<std::any>& args) override;

    std::vector<core::ArgType> read_args_type() const override;
    std::vector<core::ArgType> write_args_type() const override;

protected:
    /**
     * @brief 从可回退的输入流两遍读取网格；可覆盖以注入输入故障，验证文件入口的异常处理
     * @param input 从文件开头开始、可回退到开头的输入流，使用默认异常掩码
     * @param path 错误诊断使用的文件路径
     * @return 完整读取的网格，不返回读取失败时的部分数据
     * @throws std::runtime_error 输入流读取或回退失败
     */
    virtual std::unique_ptr<MeshData> readMesh(std::istream& input, const fs::path& path);
};

}
#endif // !M_MODEL_HANDLER_H
