/**
 * @file DockWidgetInstantiator.h
 * @brief QML 停靠面板实例化器（PreCess.Docking 的 DockWidget 类型）
 */

#pragma once

#include "core/View.h"

#include <QQuickItem>
#include <QString>

namespace dock {
class DockWidget;
}

namespace dock::qtquick {

/**
 * @brief QML 停靠面板
 *
 * QML 声明的内容子项作为 guest item；控制器在 componentComplete 时创建。
 * `source` 属性保留接口兼容，当前实现使用子项内容。
 */
class DockWidgetInstantiator : public QQuickItem, public View
{
    Q_OBJECT
    Q_PROPERTY(QString uniqueName READ uniqueName WRITE setUniqueName NOTIFY uniqueNameChanged)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(bool isOpen READ isOpen NOTIFY isOpenChanged)
    Q_PROPERTY(bool isFloating READ isFloating NOTIFY isFloatingChanged)
    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)

public:
    explicit DockWidgetInstantiator(QQuickItem* parent = nullptr);
    ~DockWidgetInstantiator() override;

    //! @brief 关联的控制器
    dock::DockWidget* dockWidget() const { return dock_widget_; }
    //! @brief guest 内容项
    QQuickItem* guestItem() const { return guest_item_; }

    QString uniqueName() const { return unique_name_; }
    void setUniqueName(const QString& unique_name);
    QString title() const;
    void setTitle(const QString& title);
    bool isOpen() const;
    bool isFloating() const;
    QString source() const { return source_; }
    void setSource(const QString& source);

    Q_INVOKABLE void open();
    Q_INVOKABLE bool close();
    Q_INVOKABLE void show();

    // View
    Controller* controller() const override;
    void setViewGeometry(const QRect& geometry) override;
    QRect viewGeometry() const override;
    void setViewVisible(bool visible) override;
    bool isViewVisible() const override;
    QSize viewMinSize() const override;
    QSize viewMaxSizeHint() const override;
    void setParentView(View* parent) override { parent_view_ = parent; }
    View* parentView() const override { return parent_view_; }
    void raiseView() override;
    void setViewCursor(Qt::CursorShape shape) override;
    Qt::CursorShape viewCursor() const override;
    QPoint viewGlobalPosition() const override;
    View* createFloatingWindowView(Controller* controller) override;

Q_SIGNALS:
    void uniqueNameChanged();
    void titleChanged();
    void isOpenChanged();
    void isFloatingChanged();
    void sourceChanged();

protected:
    void componentComplete() override;

private:
    dock::DockWidget* dock_widget_ = nullptr;
    QQuickItem* guest_item_ = nullptr;
    QString unique_name_;
    QString title_;
    QString source_;
    View* parent_view_ = nullptr;
};

}
