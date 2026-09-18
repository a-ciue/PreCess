/**
 * @file DockTypes.h
 * @brief 停靠组件的基础枚举与标志
 */

#pragma once

#include <QObject>

namespace dock {

Q_NAMESPACE

//! @brief 停靠位置（与 KDDockWidgets 命名兼容，便于 QML 迁移）
enum Location {
    Location_None,
    Location_OnLeft,
    Location_OnTop,
    Location_OnRight,
    Location_OnBottom
};
Q_ENUM_NS(Location)

//! @brief 主窗口选项
enum MainWindowOption {
    MainWindowOption_None = 0,
    //! @brief 中央为普通可停靠分组
    MainWindowOption_HasCentralGroup = 1,
    //! @brief 中央为不可分离的持久部件（含 HasCentralGroup 语义）
    MainWindowOption_HasCentralWidget = 4 | MainWindowOption_HasCentralGroup
};
Q_ENUM_NS(MainWindowOption)
Q_DECLARE_FLAGS(MainWindowOptions, MainWindowOption)

//! @brief 添加停靠部件时的初始可见性
enum InitialVisibilityOption {
    StartVisible = 0,
    StartHidden,
    PreserveCurrentTab
};
Q_ENUM_NS(InitialVisibilityOption)

//! @brief 拖放落点（内 4 + 中心 + 外 4，与 Classic 指示器一致）
enum DropLocation {
    DropLocation_None = 0,
    DropLocation_Left = 1,
    DropLocation_Top = 2,
    DropLocation_Right = 4,
    DropLocation_Bottom = 8,
    DropLocation_Center = 16,
    DropLocation_OutterLeft = 32,
    DropLocation_OutterTop = 64,
    DropLocation_OutterRight = 128,
    DropLocation_OutterBottom = 256,
    DropLocation_Inner =
        DropLocation_Left | DropLocation_Top | DropLocation_Right | DropLocation_Bottom,
    DropLocation_Outter =
        DropLocation_OutterLeft | DropLocation_OutterTop | DropLocation_OutterRight
        | DropLocation_OutterBottom
};
Q_ENUM_NS(DropLocation)
Q_DECLARE_FLAGS(DropLocations, DropLocation)

}

Q_DECLARE_OPERATORS_FOR_FLAGS(dock::MainWindowOptions)
Q_DECLARE_OPERATORS_FOR_FLAGS(dock::DropLocations)
