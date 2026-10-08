/**
 * @file AlgorithmSetup.h
 * @brief 算法注册时收集声明的可选扩展接口。
 */
#pragma once

namespace systems::algo {
class AlgorithmRegistrar;

/**
 * @brief 与 AlgorithmHandler 一同继承，在 setup 中声明算法导航。
 *
 * 保持原 AlgorithmHandler 的虚函数布局不变，旧 DLL 不实现此接口时沿用元数据。
 * setup 每次注册调用一次，仅收集声明，不写模型或创建界面。
 */
class AlgorithmSetup {
public:
    virtual ~AlgorithmSetup() = default;
    /** @brief 注册前收集完整导航声明；不得保存 registrar 引用，抛异常则注册不生效。 */
    virtual void setup(AlgorithmRegistrar& registrar) = 0;
};
}
