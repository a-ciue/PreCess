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
    stack_->undo(); // stackChanged 由 onChanged 回调发出
    emit applied();
}

void QUndoStackAdaptor::redo()
{
    if (!stack_->canRedo())
        return;
    stack_->redo();
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

bool QUndoStackAdaptor::stagedActive() const
{
    return stack_->stagedActive();
}
