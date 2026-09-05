/**
 * @file QUndoStackAdaptor.h
 * @brief undo 栈的 QML 适配器：暴露撤销/重做入口与栈状态
 */
#pragma once
#include <QObject>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>

class UndoStack;

/**
 * @brief undo 栈的 QML 适配器
 *
 * canUndo/canRedo/undoLabel/redoLabel/stagedActive 经 stackChanged 信号刷新
 * （栈内容变化含系统边界自动入栈）；stagedActive 供界面禁用导出/切换算法等入口。
 * applied 信号在 undo/redo 应用后发出，QML 侧统一清空选择集
 * （Selection 持有的 gid/稳定 id 不作跨 undo 保证）。
 */
class QUndoStackAdaptor : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("QUndoStackAdaptor is provided by QModelManager")
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY stackChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY stackChanged)
    Q_PROPERTY(QString undoLabel READ undoLabel NOTIFY stackChanged)
    Q_PROPERTY(QString redoLabel READ redoLabel NOTIFY stackChanged)
    Q_PROPERTY(bool stagedActive READ stagedActive NOTIFY stackChanged)
public:
    explicit QUndoStackAdaptor(UndoStack& stack, QObject* parent = nullptr);

    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

    bool canUndo() const;
    bool canRedo() const;
    QString undoLabel() const;
    QString redoLabel() const;
    bool stagedActive() const;

signals:
    void stackChanged(); //!< 栈内容变化（入栈/撤销/重做/清空/staged 状态变化）
    void applied(); //!< undo/redo 已应用（QML 统一清空选择集）

private:
    UndoStack* stack_;
};
