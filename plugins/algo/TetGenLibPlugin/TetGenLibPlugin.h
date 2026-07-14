/**
 * @file TetGenLibPlugin.h
 * @author 范成通 1941804585@qq.com
 * @brief TetGen 库插件入口，将 TetGenLibHandler 注册到算法系统
 * @date 2026-06-24
 */
#pragma once
#include "HandlerCreatorDestroyerFactory.h"
#include "PluginBase.h"
#include "TetGenLibHandler.h"

#include <QObject>

namespace systems::algo {
class TetGenLibPlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.algo.TetGenLibPlugin/1.0" FILE "TetGenLibPlugin.json")
private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<TetGenLibHandler, AlgorithmHandler>::get();
    }
};
}
