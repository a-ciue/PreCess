/**
 * @file DockObject.h
 * @brief 控制器基类：持有视图并提供类别标识
 */

#pragma once

#include <QObject>

namespace dock {

class DockView;

/**
 * @brief 控制器基类
 *
 * 控制器负责停靠语义与状态，界面表现通过 DockView 接口下发。
 * 视图由视图层创建并持有，控制器只保存裸指针。
 */
class DockObject : public QObject
{
    Q_OBJECT
public:
    //! @brief 控制器类别
    enum class Kind {
        None,
        Host,
        Panel,
        Group,
        Window
    };
    Q_ENUM(Kind)

    explicit DockObject(Kind kind, QObject* parent = nullptr);
    ~DockObject() override;

    //! @brief 控制器类别
    Kind kind() const { return kind_; }

    //! @brief 关联视图（可为空）
    DockView* view() const { return view_; }
    //! @brief 设置关联视图（非拥有）
    void setView(DockView* view) { view_ = view; }

private:
    Kind kind_;
    DockView* view_ = nullptr;
};

}
