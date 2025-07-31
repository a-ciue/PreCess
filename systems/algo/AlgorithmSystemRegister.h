/**
 * @file AlgorithmSystemRegister.h
 * @author (your name)
 */
#pragma once
#include "../SystemRegisterBase.h"

namespace systems::algo {
class AlgorithmSystem;
struct HandlerMetaData;

/**
 * @brief 算法系统的操作器，负责注册和注销算法插件
 */
class AlgorithmSystemRegister : public SystemRegisterBase {
public:
    AlgorithmSystemRegister(AlgorithmSystem& system);
    bool registerPlugin(const QJsonObject& meta_data, PluginBase& plugin) override;
    void unregisterPlugin(const QJsonObject& meta_data) override;

private:
    AlgorithmSystem* system_;
    HandlerMetaData toMetaData(const QJsonObject& meta_data) const;
};
}
