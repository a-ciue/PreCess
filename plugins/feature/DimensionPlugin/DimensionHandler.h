/**
 * @file DimensionHandler.h
 * @brief 尺寸标注处理器声明：距离、角度、半径、长度、面积、体积、包围盒与重心
 * @author 范成通 email 1941804585@qq.com
 */
#pragma once
#include "FeatureHandler.h"

namespace systems::feature {

/**
 * @brief 尺寸标注处理器：功能参数（测量类型 + 选择对象）+ 菜单执行 → 返回结果文本
 *
 * 纯参数执行功能：setup()/execute() 均在 GUI 线程调用，execute 无状态，
 * 与视口交互测量（MeasurePlugin）相互独立。
 */
class DimensionHandler : public FeatureHandler {
public:
    DimensionHandler() = default;
    ~DimensionHandler() override = default;

    //! @brief 声明功能参数（测量类型、选择对象）与菜单项
    void setup(FeatureRegistrar& reg) override;
    //! @brief 尺寸标注：按参数中的测量类型与选择对象执行，返回结果文本
    std::any execute(FeatureContext& ctx) override;
};
}
