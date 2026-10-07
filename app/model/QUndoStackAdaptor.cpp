/**
 * @file QUndoStackAdaptor.cpp
 */
#include "QUndoStackAdaptor.h"
#include "UndoStack.h"

QUndoStackAdaptor::QUndoStackAdaptor(UndoStack& stack, QObject* parent)
    : QObject(parent)
    , stack_(&stack)
{
    // 栈内容变化（含系统边界自动入栈）同步 QML 属性
    stack_->setOnChanged([this] { emit stackChanged(); });
}

void QUndoStackAdaptor::undo()
{
    if (!stack_->canUndo())
        return;
    if (stack_->undo()) // 只有真正恢复模型才通知选择集清空。
        emit applied();
}

void QUndoStackAdaptor::redo()
{
    if (!stack_->canRedo())
        return;
    if (stack_->redo())
        emit applied();
}

bool QUndoStackAdaptor::canUndo() const
{
    return stack_->canUndo();
}

bool QUndoStackAdaptor::canRedo() const
{
    return stack_->canRedo();
}

QString QUndoStackAdaptor::undoLabel() const
{
    const auto label = stack_->undoLabel();
    return label ? QString::fromStdString(*label) : QString();
}

QString QUndoStackAdaptor::redoLabel() const
{
    const auto label = stack_->redoLabel();
    return label ? QString::fromStdString(*label) : QString();
}

bool QUndoStackAdaptor::scopeActive() const
{
    return stack_->scopeActive();
}
