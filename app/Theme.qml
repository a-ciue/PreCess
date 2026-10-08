/**
 * @file Theme.qml
 * @brief 全局设计令牌单例：语义色、字号、间距与圆角
 *
 * 颜色一律按角色命名（primary / surface / border / textXxx），
 * 应用界面统一引用本文件；独立 dock 模块的局部配色需同步检查。
 * 字号按紧凑桌面工具取模数音阶：基准 13px、小二度（1.125）取整。
 */

pragma Singleton
import QtQuick

QtObject {
    // 修改配色时同步检查 dock/ui/qml/PanelGroup.qml 与 DockWindow.qml 的局部配色。
    // ---- 交互色（延续既有交互蓝 #1976D2）----
    readonly property color primary: "#1976D2"
    readonly property color primaryHover: "#1565C0"
    readonly property color primaryPressed: "#0F5AAD"
    //! 选中行 / 选中态的淡蓝着色
    readonly property color primaryTint: "#E3F0FB"
    readonly property color textOnPrimary: "#FFFFFF"

    // ---- 反馈色 ----
    readonly property color danger: "#D32F2F"
    readonly property color dangerTint: "#FDECEA"

    // ---- 面与背景 ----
    //! 应用底色（中央区、ribbon 页背景）
    readonly property color windowBackground: "#F3F4F6"
    //! 面板 / 菜单等前景面
    readonly property color surface: "#FFFFFF"
    //! 面板标题栏、表头等次级面
    readonly property color surfaceAlt: "#F7F8FA"
    //! 停靠面板标题面，与内容区拉开层次
    readonly property color panelHeaderSurface: "#E6EDF5"
    //! 列表项悬停着色
    readonly property color hoverOverlay: "#ECF0F4"

    // ---- 边框 ----
    readonly property color border: "#D9DDE3"
    readonly property color borderStrong: "#C3C8CF"

    // ---- 文本 ----
    readonly property color textPrimary: "#2B2F36"
    readonly property color textSecondary: "#6B7280"
    readonly property color textDisabled: "#9CA3AF"

    // ---- 滚动条 ----
    readonly property color scrollBarIdle: "#D5D9DF"
    readonly property color scrollBarHover: "#B7BDC7"

    // ---- 字号（基准 13，小二度音阶取整）----
    readonly property int fontSizeLarge: 15
    readonly property int fontSizeBody: 13
    readonly property int fontSizeCaption: 12
    readonly property int fontSizeSmall: 11
    //! 等宽字体（树节点名、控制台、日志）
    readonly property string monoFamily: "Consolas"

    // ---- 间距 ----
    readonly property int spacingXs: 4
    readonly property int spacingSm: 8
    readonly property int spacingMd: 12
    readonly property int spacingLg: 16

    // ---- 圆角 ----
    readonly property int radiusControl: 4
    readonly property int radiusMenu: 6
}
