/**
 * @file Theme.qml
 * @brief 全局设计令牌单例：深浅主题语义色、字号、间距与圆角
 *
 * 颜色按角色命名（primary / surface / border / textXxx），界面统一引用本文件。
 * 明暗由 Theme.mode 驱动并持久化于 QSettings，每个颜色令牌按 dark 求值，
 * 切换后所有绑定点自动更新；独立 dock 模块的局部配色经 Docking.Theme 同步。
 * 字号按紧凑桌面工具取模数音阶：基准 13px、小二度（1.125）取整。
 */

pragma Singleton
import QtQuick
import QtCore

QtObject {
    id: theme

    //! @brief 主题模式
    enum Mode { Light, Dark }

    // 持久化真源；QtObject 无默认属性，故经属性持有 Settings。
    // mode 绑定于其 mode，修改请用 setMode 写回（无重复回写）。
    property var _settings: Settings {
        category: "Appearance"
        property int mode: Theme.Mode.Light
    }

    //! @brief 当前主题模式（只读，修改用 setMode）
    readonly property int mode: _settings.mode
    //! @brief 是否深色主题
    readonly property bool dark: mode === Theme.Mode.Dark

    /**
     * @brief 设置主题模式
     * @param m Theme.Mode.Light 或 Theme.Mode.Dark
     */
    function setMode(m) {
        _settings.mode = m
    }

    // 修改配色时同步检查 dock/ui/qml 的局部配色（经 Docking.Theme.dark 选择）。
    // ---- 交互色（浅色延续既有交互蓝 #1976D2，深色提亮）----
    readonly property color primary: dark ? "#4EA1F3" : "#1976D2"
    readonly property color primaryHover: dark ? "#6BB3F5" : "#1565C0"
    readonly property color primaryPressed: dark ? "#3B8CD6" : "#0F5AAD"
    //! 选中行 / 选中态的淡蓝着色
    readonly property color primaryTint: dark ? "#1B3A5C" : "#E3F0FB"
    readonly property color textOnPrimary: "#FFFFFF"

    // ---- 反馈色 ----
    readonly property color danger: dark ? "#F14C4C" : "#D32F2F"
    readonly property color dangerTint: dark ? "#3A2020" : "#FDECEA"
    //! 状态栏等处的警示文字
    readonly property color warning: dark ? "#FFA657" : "#EF6C00"

    // ---- 面与背景 ----
    //! 应用底色（中央区、ribbon 页背景）
    readonly property color windowBackground: dark ? "#1E1E1E" : "#F3F4F6"
    //! 面板 / 菜单等前景面
    readonly property color surface: dark ? "#252526" : "#FFFFFF"
    //! 面板标题栏、表头等次级面
    readonly property color surfaceAlt: dark ? "#2D2D30" : "#F7F8FA"
    //! 停靠面板标题面，与内容区拉开层次
    readonly property color panelHeaderSurface: dark ? "#333336" : "#E6EDF5"
    //! 列表项悬停着色
    readonly property color hoverOverlay: dark ? "#2A2D2E" : "#ECF0F4"
    //! 忙碌遮罩层
    readonly property color busyMaskOverlay: dark ? Qt.rgba(0.12, 0.12, 0.12, 0.55)
                                                 : Qt.rgba(0.93, 0.93, 0.93, 0.55)

    // ---- 边框 ----
    readonly property color border: dark ? "#3E3E42" : "#D9DDE3"
    readonly property color borderStrong: dark ? "#4A4A4F" : "#C3C8CF"

    // ---- 文本 ----
    readonly property color textPrimary: dark ? "#D4D4D4" : "#2B2F36"
    readonly property color textSecondary: dark ? "#9DA1A6" : "#6B7280"
    readonly property color textDisabled: dark ? "#6B6F76" : "#9CA3AF"

    // ---- 滚动条 ----
    readonly property color scrollBarIdle: dark ? "#424242" : "#D5D9DF"
    readonly property color scrollBarHover: dark ? "#5A5A5A" : "#B7BDC7"

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
