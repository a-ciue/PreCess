/**
 * @file DockingAreaView.h
 * @brief QML 主停靠区域（PreCess.Docking 的 DockingArea 类型）
 */

#pragma once

#include "core/DockTypes.h"
#include "core/View.h"

#include <QQuickItem>
#include <QUrl>

#include <memory>

namespace dock {
class MainWindow;

namespace qtquick {

class AreaItem;
class GuestView;

/**
 * @brief 主停靠区域视图
 *
 * 对应 QML 的 DockingArea：创建 MainWindow 控制器与布局区域视图，
 * 加载 central 持久部件，并把 QML 的 addDockWidget 调用转成命令式停靠。
 */
class DockingAreaView : public QQuickItem, public View
{
    Q_OBJECT
    Q_PROPERTY(QString uniqueName READ uniqueName WRITE setUniqueName NOTIFY uniqueNameChanged)
    Q_PROPERTY(int options READ options WRITE setOptions NOTIFY optionsChanged)
    Q_PROPERTY(QUrl persistentCentralItemFileName READ persistentCentralItemFileName WRITE
            setPersistentCentralItemFileName NOTIFY persistentCentralItemFileNameChanged)

public:
    explicit DockingAreaView(QQuickItem* parent = nullptr);
    ~DockingAreaView() override;

    QString uniqueName() const { return unique_name_; }
    void setUniqueName(const QString& unique_name);

    int options() const { return options_; }
    void setOptions(int options);

    QUrl persistentCentralItemFileName() const { return central_file_; }
    void setPersistentCentralItemFileName(const QUrl& file);

    //! @brief 命令式停靠（对应 MainWindow::addDockWidget）
    Q_INVOKABLE void addDockWidget(QQuickItem* dock_widget, int location,
        QQuickItem* relative_to = nullptr, QSize preferred_size = QSize(), int option = StartVisible);
    //! @brief 以选项卡方式加入中央分组
    Q_INVOKABLE void addDockWidgetAsTab(QQuickItem* dock_widget);

    // View
    Controller* controller() const override;
    void setViewGeometry(const QRect& geometry) override;
    QRect viewGeometry() const override;
    void setViewVisible(bool visible) override;
    bool isViewVisible() const override;
    QSize viewMinSize() const override;
    QSize viewMaxSizeHint() const override;
    void setParentView(View* parent) override { Q_UNUSED(parent); }
    View* parentView() const override { return nullptr; }
    void raiseView() override;
    void setViewCursor(Qt::CursorShape shape) override;
    Qt::CursorShape viewCursor() const override;
    QPoint viewGlobalPosition() const override;
    View* createFloatingWindowView(Controller* controller) override;

Q_SIGNALS:
    void uniqueNameChanged();
    void optionsChanged();
    void persistentCentralItemFileNameChanged();

protected:
    void componentComplete() override;
    void geometryChange(const QRectF& new_geometry, const QRectF& old_geometry) override;

private:
    //! @brief 同步区域几何到控制器与视图
    void updateAreaGeometry();
    //! @brief 监听宿主窗口移动（全局原点变化）
    void watchWindow();
    //! @brief 加载 central 持久部件
    void loadCentralItem();

    QString unique_name_;
    int options_ = 0;
    QUrl central_file_;
    dock::MainWindow* main_window_ = nullptr;
    AreaItem* area_item_ = nullptr;
    std::unique_ptr<GuestView> central_view_;
    bool window_watched_ = false;
};

}
}
