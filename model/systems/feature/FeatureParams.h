/**
 * @file FeatureParams.h
 * @brief 功能的持久参数集
 */
#ifndef FEATURE_PARAMS_H
#define FEATURE_PARAMS_H
#include "ArgObject.h"
#include "ArgType.h"

#include <cstddef>
#include <vector>

namespace systems::feature {
/**
 * @brief 功能的持久参数集：参数类型与当前值一一对应
 *
 * 与算法系统的一次性传参不同，功能参数在功能注册后长期存在，
 * 初始值取自 ArgType::content，修改经 FeatureSystem::setParameter 生效并广播事件。
 */
class FeatureParams {
public:
    explicit FeatureParams(std::vector<core::ArgType> types);

    const std::vector<core::ArgType>& types() const noexcept { return types_; }
    std::size_t count() const noexcept { return types_.size(); }
    /**
     * @brief 读取参数当前值
     * @throw std::out_of_range 下标越界
     */
    const core::ArgObject& value(std::size_t index) const;
    /**
     * @brief 设置参数当前值
     * @throw std::out_of_range 下标越界
     */
    void setValue(std::size_t index, core::ArgObject new_value);

private:
    std::vector<core::ArgType> types_;
    std::vector<core::ArgObject> values_;
};
}
#endif // FEATURE_PARAMS_H
