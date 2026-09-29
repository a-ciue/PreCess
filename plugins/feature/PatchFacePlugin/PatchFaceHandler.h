#pragma once
#include "FeatureHandler.h"

namespace systems::feature {
/**
 * @brief 查找包含选中边的最小闭合边界环，并创建一个补面。
 */
class PatchFaceHandler : public FeatureHandler {
public:
    //! @brief 声明边界选择参数及补面菜单入口。
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override;
    //! @brief 根据选中边解析组件，构造最小环补面并写回模型。
    std::any execute(FeatureContext& ctx) override;
};
}
