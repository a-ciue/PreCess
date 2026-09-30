/**
 * @file DockHostItem.h
 * @brief QML 主停靠区域（PreCess.Docking 的 DockHost 类型）
 */

#pragma once

#include "docking/DockEnums.h"
#include "docking/DockView.h"

#include <QQuickItem>
#include <QSet>
#include <QUrl>

#include <memory>

class QQuickWindow;

namespace dock {
class DockHost;
class DockPanel;

namespace ui {

class DockAreaItem;
class PanelContentView;

/**
 * @brief 主停靠区域视图
 *
 * 对应 QML 的 DockHost：创建 DockHost 控制器与布局区域视图，
 * 加载 central 持久部件，并把 QML 的 placePanel 调用转成命令式停靠。
 */
class DockHostItem : public QQuickItem, public DockView
{
    Q_OBJECT
    Q_PROPERTY(QString uniqueName READ uniqueName WRITE setUniqueName NOTIFY uniqueNameChanged)
    Q_PROPERTY(QUrl centralItemFile READ centralItemFile WRITE setCentralItemFile NOTIFY centralItemFileChanged)

public:
    explicit DockHostItem(QQuickItem* parent = nullptr);
    ~DockHostItem() override;

    QString uniqueName() const { return unique_name_; }
    void setUniqueName(const QString& unique_name);

    QUrl centralItemFile() const { return central_file_; }
    void setCentralItemFile(const QUrl& file);

    //! @brief 命令式放置面板（对应 DockHost::placePanel）
    Q_INVOKABLE void placePanel(QQuickItem* panel, DockEdge edge,
        QQuickItem* relative_to = nullptr, QSize preferred_size = QSize(),
        PanelLaunch launch = PanelLaunch::Visible);
    //! @brief 序列化当前布局为 JSON 字符串
    Q_INVOKABLE QString saveLayout();
    //! @brief 从 JSON 字符串恢复布局；失败返回 false
    Q_INVOKABLE bool restoreLayout(const QString& layout);

    // DockView
    DockObject* dockObject() const override;
    void applyFrame(const QRect& geometry) override;
    QRect frame() const override;
    void applyVisibility(bool visible) override;
    bool isShown() const override;
    QSize minExtent() const override;
    QSize maxExtent() const override;
    void bringToFront() override;
    QPoint globalOrigin() const override;
    DockView* createDockWindow(DockObject* controller) override;

Q_SIGNALS:
    void uniqueNameChanged();
    void centralItemFileChanged();

protected:
    void componentComplete() override;
    void geometryChange(const QRectF& new_geometry, const QRectF& old_geometry) override;
    void itemChange(ItemChange change, const ItemChangeData& value) override;

private:
    //! @brief 同步区域几何到控制器与视图
    void updateAreaGeometry();
    //! @brief 监听宿主窗口移动（全局原点变化）；窗口变化时自动重绑
    void watchWindow();
    //! @brief 加载 central 持久部件
    void loadCentralItem();

    QString unique_name_;
    QUrl central_file_;
    dock::DockHost* host_ = nullptr;
    DockAreaItem* area_item_ = nullptr;
    std::unique_ptr<PanelContentView> central_view_;
    QQuickWindow* watched_window_ = nullptr;
    //! @brief 已绑定视图同步的面板（重复 placePanel 不累积连接）
    QSet<dock::DockPanel*> place_watches_;
};

}
}
