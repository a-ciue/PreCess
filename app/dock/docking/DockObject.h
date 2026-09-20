/**
 * @file DockObject.h
 * @brief 控制器基类：持有视图
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
    explicit DockObject(QObject* parent = nullptr);
    ~DockObject() override;

    //! @brief 关联视图（可为空）
    DockView* view() const { return view_; }
    //! @brief 设置关联视图（非拥有）
    void setView(DockView* view) { view_ = view; }

private:
    DockView* view_ = nullptr;
};

}
