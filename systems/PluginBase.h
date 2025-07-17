/**
 * @file PluginBase.h
 * @author 张家僮(htxz_6a6@163.com)
 */
#ifndef PLUGIN_BASE_H
#define PLUGIN_BASE_H
#include <any>

namespace systems {
class PluginBase {
public:
    virtual ~PluginBase() = default;
    virtual std::any makeHandler() = 0;
};
}

#ifdef Q_MOC_RUN
Q_DECLARE_INTERFACE(systems::PluginBase, "com.PreCess.systems.PluginBase/1.0")
#endif

#endif // PLUGIN_BASE_H
