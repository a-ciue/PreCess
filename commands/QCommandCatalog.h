#pragma once
#include <QObject>
#include <qqmlintegration.h>
#include <QList>
#include "QCommand.h"

/**
 * @brief QCommandCatalog 命令列表储存命令，负责命令的注册与管理
 *
 * QCommandCatalog 保存所有可用的命令（QCommand 对象），并提供接口将命令列表暴露给 QML 层。
 * 通过该类，可以在程序初始化时注册各种命令，然后在 QML 中获取命令列表用于展示和触发执行。
 */
class QCommandCatalog : public QObject {
    Q_OBJECT
    QML_ELEMENT
public:
    /**
     * @brief 构造 QCommandCatalog 对象
     * @param parent 父对象，默认为 nullptr
     */
    explicit QCommandCatalog(QObject* parent = nullptr) : QObject(parent) {}

    /**
     * @brief 向命令目录中添加一个命令
     * @param cmd 指向要注册的 QCommand 对象指针
     *
     * 将一个命令注册到目录中。该函数会设置命令对象的父对象为当前 QCommandCatalog，以便统一管理其生命周期。
     */
    void addCommand(QCommand* cmd)
    {
        if (!cmd)
            return;

        if (const QString& path = cmd->path(); !path.isEmpty())
        {
            // 检查命令名称是否已存在
            if (command_map_.count(path)) {
                qWarning() << "命令名称已存在，无法注册:" << path;
            }
            else
            {
                command_map_[path] = cmd;
            }
        }

        cmd->setParent(this);
        commands_.append(cmd);
    }

    /**
     * @brief 根据命令路径获取对应的命令对象
     *
     * 通过命令路径查找并返回对应的命令对象。
     * @param path 命令路径
     * @return 指向对应 QCommand 对象的指针，如果不存在则返回 nullptr
     */
    Q_INVOKABLE QCommand* pathCommand(const QString& path)
    {
        auto it = command_map_.find(path);
        if (it != command_map_.end()) {
            return it->second;
        }
        return nullptr;
    }

    /**
     * @brief 获取当前注册的命令列表（供 QML 调用）
     * @return 包含所有命令对象的 QList<QObject*> 列表
     *
     * 返回一个命令对象列表，可用于在 QML 中显示所有可用命令。
     */
    Q_INVOKABLE QList<QCommand*> qmlCommands() const {
        return commands_;
    }

private:
    QList<QCommand*> commands_;   //!< 已注册的命令对象列表
    std::unordered_map<QString, QCommand*> command_map_; //!< 命令名称与命令对象的映射
};
