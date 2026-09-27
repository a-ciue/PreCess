#ifndef Q_PYTHON_RUNTIME_QML_H
#define Q_PYTHON_RUNTIME_QML_H

#include "QPythonRuntime.h"

#include <QtQml/qqml.h>

/**
 * @brief QPythonRuntime 的 QML 外来类型包装：类本体编入 pythonApp 静态库
 * （不携带 QML 宏），QML 注册经本包装在 modelQml 的 app.model 模块完成
 *
 * 名称与不可创建语义与迁出 modelQml 前一致：QML 中类型名为 QPythonRuntime，
 * 仅可经 QModelManager.pythonRuntime 属性访问、不可声明。
 */
struct QPythonRuntimeQml {
    Q_GADGET
    QML_FOREIGN(QPythonRuntime)
    QML_NAMED_ELEMENT(QPythonRuntime)
    QML_UNCREATABLE("经 QModelManager.pythonRuntime 访问")
};

#endif
