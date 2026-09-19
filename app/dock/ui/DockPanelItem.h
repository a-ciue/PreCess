/**
 * @file DockPanelItem.h
 * @brief QML 停靠面板实例化器（PreCess.Docking 的 DockPanel 类型）
 */

#pragma once

#include "docking/DockView.h"

#include <QQuickItem>
#include <QString>

namespace dock {
class DockPanel;
}

namespace dock::ui {

/**
 * @brief QML 停靠面板
 *
 * QML 声明的内容子项作为 client item；控制器在 componentComplete 时创建。
 * `source` 属性保留接口兼容，当前实现使用子项内容。
 */
class DockPanelItem : public QQuickItem, public DockView
{
    Q_OBJECT
    Q_PROPERTY(QString uniqueName READ uniqueName WRITE setUniqueName NOTIFY uniqueNameChanged)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(bool shown READ isPanelShown NOTIFY shownChanged)
    Q_PROPERTY(bool isDetached READ isDetached NOTIFY detachedChanged)
    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)

public:
    explicit DockPanelItem(QQuickItem* parent = nullptr);
    ~DockPanelItem() override;

    //! @brief 关联的控制器
    dock::DockPanel* panel() const { return panel_; }
    //! @brief client 内容项
    QQuickItem* panelContentItem() const { return panel_content_item_; }

    QString uniqueName() const { return unique_name_; }
    void setUniqueName(const QString& unique_name);
    QString title() const;
    void setTitle(const QString& title);
    //! @brief QML `shown` 属性：面板是否显示
    bool isPanelShown() const;
    bool isDetached() const;
    QString source() const { return source_; }
    void setSource(const QString& source);

    Q_INVOKABLE void showPanel();
    Q_INVOKABLE void hidePanel();

    // DockView
    DockObject* dockObject() const override;
    void applyFrame(const QRect& geometry) override;
    QRect frame() const override;
    void applyVisibility(bool visible) override;
    bool isShown() const override;
    QSize minExtent() const override;
    QSize maxExtent() const override;
    void setParentDockView(DockView* parent) override { parent_view_ = parent; }
    DockView* parentDockView() const override { return parent_view_; }
    void bringToFront() override;
    void setCursorShape(Qt::CursorShape shape) override;
    Qt::CursorShape cursorShape() const override;
    QPoint globalOrigin() const override;
    DockView* createDockWindow(DockObject* controller) override;

Q_SIGNALS:
    void uniqueNameChanged();
    void titleChanged();
    void shownChanged();
    void detachedChanged();
    void sourceChanged();

protected:
    void componentComplete() override;

private:
    dock::DockPanel* panel_ = nullptr;
    QQuickItem* panel_content_item_ = nullptr;
    QString unique_name_;
    QString title_;
    QString source_;
    DockView* parent_view_ = nullptr;
};

}
