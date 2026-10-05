#pragma once
#include "FeatureHandler.h"

namespace systems::feature {
/**
 * @brief 拉伸面为实体：插件复制截面、后台计算，GUI 追加实体，源面保留。
 * @note 宿主须装配共享 JobRunner；最终结果文本经 FeatureResultEvent 发布。
 */
class ExtrudeFaceHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar& reg, FeatureContext& ctx) override;
    std::any execute(FeatureContext& ctx) override;
};
}
