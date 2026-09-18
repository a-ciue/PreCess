/**
 * @file Controller.h
 * @brief 控制器基类：持有视图并提供类型标识
 */

#pragma once

#include <QObject>

namespace dock {

class View;

/**
 * @brief 控制器基类
 *
 * 控制器负责停靠语义与状态，界面表现通过 View 接口下发。
 * 视图由视图层创建并持有，控制器只保存裸指针。
 */
class Controller : public QObject
{
    Q_OBJECT
public:
    //! @brief 控制器类型
    enum class Type {
        None,
        MainWindow,
        DockWidget,
        Group,
        FloatingWindow,
        TitleBar,
        DragIndicator
    };
    Q_ENUM(Type)

    explicit Controller(Type type, QObject* parent = nullptr);
    ~Controller() override;

    //! @brief 控制器类型
    Type type() const { return type_; }

    //! @brief 关联视图（可为空）
    View* view() const { return view_; }
    //! @brief 设置关联视图（非拥有）
    void setView(View* view) { view_ = view; }

private:
    Type type_;
    View* view_ = nullptr;
};

}
