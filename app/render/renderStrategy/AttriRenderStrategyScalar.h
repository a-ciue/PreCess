#pragma once
#include "IAttributeRenderStrategy.h"
#include "AttributeOperator.h"

class vtkPolyDataMapper;
class vtkScalarBarActor;

class AttriRenderStrategyScalar : public IAttributeRenderStrategy {
public:
    // 接收渲染窗口共享的颜色表，仅负责标量渲染时配置和显示，不管理其生命周期。
    explicit AttriRenderStrategyScalar(vtkScalarBarActor* scalar_bar = nullptr);

    void render(
        AttributeOperator op,
        const std::string& attr_name,
        std::map<std::string, std::any> args) override;

private:
    // 使用 mapper 的颜色映射和值域更新颜色表。
    void showScalarBar(
        vtkPolyDataMapper* mapper,
        const std::string& title,
        const double range[2]);

    // 指向渲染窗口共享的颜色表，不负责其生命周期。
    vtkScalarBarActor* scalar_bar_ {};
};