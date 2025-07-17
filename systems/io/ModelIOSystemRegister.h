/**
 * @brief ModelIOSystemRegister.h
 * @author 张家僮(htxz_6a6@163.com)
 */
#ifndef MODEL_IO_SYSTEM_REGISTER_H
#define MODEL_IO_SYSTEM_REGISTER_H

#include "../SystemRegisterBase.h"

namespace systems::io {
class ModelIOSystem;
struct HandlerMetaData;

/**
 * @brief ModelIOSystem的操作类，负责功能注册和管理
 */
class ModelIOSystemRegister : public SystemRegisterBase { 
public:
    ModelIOSystemRegister(ModelIOSystem& system);
    bool registerHandler(const QJsonObject& meta_data, std::any handler) override;
    void unregisterHandler(const QJsonObject& meta_data) override;

private:
    ModelIOSystem* system_;

    HandlerMetaData toMetaData(const QJsonObject& meta_data) const;
};
}

#endif // MODEL_IO_SYSTEM_REGISTER_H