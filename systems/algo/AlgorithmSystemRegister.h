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
 * @brief 算法系统的操作器，负责注册和注销算法处理器Handler
 */
class AlgorithmSystemRegister : public SystemRegisterBase {
public:
    AlgorithmSystemRegister(AlgorithmSystem& system);
    bool registerHandler(const QJsonObject& meta_data, std::any handler) override;
    void unregisterHandler(const QJsonObject& meta_data) override;

private:
    AlgorithmSystem* system_;
    HandlerMetaData toMetaData(const QJsonObject& meta_data) const;
};
}
