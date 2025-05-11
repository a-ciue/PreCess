#pragma once
#include <QObject>
#include <qqmlintegration.h>
#include <QString>
#include <QStandardItemModel>
#include <QVariant>
#include <functional>
#include <memory>
#include <qqmlregistration.h>

#include "ArgTypeObject.h"
#include "ICommand.h"

#include "../ModelOperator.h"

/**
 * @brief 对应一个ICommand派生类，用于在QML中展示命令和需要的参数
 *
 * 每个 QCommand 对象表示一种可执行的命令，包括命令名称、参数类型以及执行该命令的工厂方法。
 * QCommand 可用于在 QML 中展示命令（名称和参数要求），并通过工厂方法根据提供的 ModelOperator 和参数构造具体的命令(ICommand)实例。
 */
class QCommand : public QObject {
Q_OBJECT
    QML_ELEMENT
Q_PROPERTY(QList<ArgTypeObject*> arg_types MEMBER arg_types_ READ getArgTypes)
public:
    /// 定义命令工厂函数类型：传入 ModelOperator 和参数列表，返回命令对象智能指针
    using CommandFactory = std::function<std::unique_ptr<ICommand>(ModelOperator, const QVariantList&)>;

    /**
     * @brief 构造 QCommand 对象
     * @param name 命令名称（用于在界面显示）
     * @param factory 命令对应的工厂函数，用于创建具体 ICommand 对象
     * @param arg_types 命令参数类型描述模型（可选），用于描述该命令所需参数
     * @param parent 父对象，默认为 nullptr
     */
    QCommand(const QString& name, CommandFactory factory, QList<ArgTypeObject*> arg_types, QObject* parent = nullptr)
            : QObject(parent), name_(name), factory_(factory), arg_types_(arg_types)
    {
    }

    /**
     * @brief 获取命令名称
     * @return 命令名称字符串
     */
    Q_INVOKABLE QString name() const { return name_; }

    /**
     * @brief 获取命令参数类型模型
     * @return 指向描述命令参数类型的 QStandardItemModel 对象指针
     */
    QList<ArgTypeObject*> getArgTypes() const
    {
	    return arg_types_;
    }

    /**
     * @brief 基于当前命令的工厂创建具体命令实例
     * @param modelOp 执行命令所针对的模型操作对象(ModelOperator)
     * @param args 命令执行所需的参数列表
     * @return 包含具体命令对象的智能指针，如果创建失败则返回 nullptr
     */
    std::unique_ptr<ICommand> create(ModelOperator modelOp, const QVariantList& args) {
        return factory_ ? factory_(modelOp, args) : nullptr;
    }

private:
    QString name_;                                 //!< 命令名称，用于在 QML 中显示
    CommandFactory factory_;                       //!< 用于创建具体命令的工厂函数
    QList<ArgTypeObject*> arg_types_;              //!< 命令参数类型描述模型指针
};
