/**
 * @file AlgorithmSystemRegister.cpp
 * @author (your name)
 */
#include "AlgorithmSystemRegister.h"
#include "AlgorithmSystem.h"
#include <QJsonArray>
#include <cassert>

namespace systems::algo {
AlgorithmSystemRegister::AlgorithmSystemRegister(AlgorithmSystem& system) : system_(&system) {}

bool AlgorithmSystemRegister::registerHandler(const QJsonObject& meta_data, std::any handler) {
    assert(system_);
    auto handler_p = std::any_cast<std::shared_ptr<AlgorithmSystem::Handler>>(&handler);
    if (!handler_p) return false;
    auto handler_data = toMetaData(meta_data);
    return system_->registerHandler(handler_data, *handler_p);
}

void AlgorithmSystemRegister::unregisterHandler(const QJsonObject& meta_data) {
    assert(system_);
    auto handler_data = toMetaData(meta_data);
    system_->unregisterHandler(handler_data);
}

HandlerMetaData AlgorithmSystemRegister::toMetaData(const QJsonObject& meta_data) const {
    HandlerMetaData handle_data;
    handle_data.name = meta_data.value("name").toString().toStdString();
    handle_data.display_name = meta_data.value("display_name").toString().toStdString();
    return handle_data;
}
}
