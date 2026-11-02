#pragma once
#include "IAttributeRenderStrategy.h"
#include "AttributeOperator.h"

class vtkPolyDataMapper;
class vtkScalarBarActor;
struct ScalarDisplayConfig;

class AttriRenderStrategyScalar : public IAttributeRenderStrategy {
public:
    // 接收渲染窗口共享的颜色表，仅负责标量渲染时配置和显示，不管理其生命周期。
    explicit AttriRenderStrategyScalar(vtkScalarBarActor* scalar_bar = nullptr);

    void render(
        AttributeOperator op,
        const std::string& attr_name,
        std::map<std::string, std::any> args) override;

    /** @brief 直接使用类型化配置执行标量着色，与旧参数适配入口共用同一实现。 */
    void render(AttributeOperator op, const ScalarDisplayConfig& config);

private:
    // 使用 mapper 的颜色映射和值域更新颜色表。
    void showScalarBar(
        vtkPolyDataMapper* mapper,
        const std::string& title,
        const double range[2]);

    // 指向渲染窗口共享的颜色表，不负责其生命周期。
    vtkScalarBarActor* scalar_bar_ {};
};
