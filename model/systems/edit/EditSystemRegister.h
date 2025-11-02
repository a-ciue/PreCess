/**
 * @file EditSystemRegister.h
 * @author 张家僮(htxz_6a6@163.com)
 */
#pragma once
#include "SystemRegisterBase.h"

namespace systems::edit {
class EditSystem;
struct HandlerMetaData;

/**
 * @brief 模型编辑系统的操作器，负责注册和注销模型编辑插件
 */
class EditSystemRegister : public SystemRegisterBase {
public:
    EditSystemRegister(EditSystem& system);
    bool registerPlugin(const QJsonObject& meta_data, PluginBase& plugin) override;
    void unregisterPlugin(const QJsonObject& meta_data) override;

private:
    EditSystem* system_;
    HandlerMetaData toMetaData(const QJsonObject& meta_data) const;
};
}
