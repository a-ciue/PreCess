//
// Created by 徐昊阳 on 4/12/25.
//
// command/ICommand.h
#pragma once

/**
 * ICommand：所有命令的抽象基类接口
 */
class ICommand {
public:
    virtual ~ICommand() = default;
    /** 执行命令操作 */
    virtual void execute() = 0;
    /** 撤销命令操作（预留，支持 undo/redo） */
    virtual void undo() = 0;
    /** 重做命令操作（预留） */
    virtual void redo() = 0;
};
