/**
 * @file FeatureSystemRegister.h
 */
#pragma once
#include "SystemRegisterBase.h"

namespace systems::feature {
class FeatureSystem;
struct HandlerMetaData;

/**
 * @brief 功能系统的注册器，负责注册和注销功能插件
 */
class FeatureSystemRegister : public SystemRegisterBase {
public:
    FeatureSystemRegister(FeatureSystem& system);
    bool registerPlugin(const QJsonObject& meta_data, PluginBase& plugin) override;
    void unregisterPlugin(const QJsonObject& meta_data) override;

private:
    FeatureSystem* system_;
    HandlerMetaData toMetaData(const QJsonObject& meta_data) const;
};
}
