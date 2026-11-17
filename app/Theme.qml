/**
 * @file Theme.qml
 * @brief 全局设计令牌单例：深浅主题语义色、字号、间距与圆角
 *
 * 颜色按角色命名（primary / surface / border / textXxx），界面统一引用本文件。
 * 明暗由 Theme.mode 驱动并持久化于 QSettings，每个颜色令牌按 dark 求值，
 * 切换后所有绑定点自动更新。调色板角色对应关系与 dock 配色表也在此集中定义，
 * 由 Main.qml 推送给 AppPalette 与 Docking.Theme；其他模块只接收和展示。
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
        // 自定义壁纸 id（托管目录内文件名，空串 = 主题纯色默认）
        property string wallpaper: ""
    }

    //! @brief 当前主题模式（只读，修改用 setMode）
    readonly property int mode: _settings.mode
    //! @brief 是否深色主题
    readonly property bool dark: mode === Theme.Mode.Dark
    //! @brief 自定义壁纸 id（只读，修改用 setWallpaper；空串 = 纯色默认）
    readonly property string wallpaper: _settings.wallpaper
    //! @brief 是否启用自定义壁纸（决定面板是否透出背景）
    readonly property bool hasWallpaper: wallpaper.length > 0
    //! @brief 壁纸当前是否可显示（运行时状态，不持久化；加载失败时仅降级面色）
    property bool wallpaperReady: true
    //! @brief 运行时重载令牌：仅用于打破 QML 图像缓存，不持久化
    property int wallpaperReloadToken: 0
    //! @brief 面板是否按壁纸半透明：选择存在且图像已就绪
    readonly property bool wallpaperSurfacesActive: hasWallpaper && wallpaperReady
    //! @brief 壁纸图像源（经 image://wallpaper/ 解析到托管目录；空串 = 无壁纸）
    readonly property url wallpaperSource: wallpaper.length > 0
        ? "image://wallpaper/" + wallpaper + (wallpaperReloadToken > 0 ? "?v=" + wallpaperReloadToken : "")
        : ""

    /**
     * @brief 设置主题模式
     * @param m Theme.Mode.Light 或 Theme.Mode.Dark
     */
    function setMode(m) {
        _settings.mode = m
    }

    /**
     * @brief 设置自定义壁纸
     * @param id 托管壁纸 id（空串表示回退主题纯色）
     */
    function setWallpaper(id) {
        if (_settings.wallpaper !== id)
            wallpaperReloadToken = 0
        _settings.wallpaper = id
    }

    /** @brief 同一壁纸 id 显示失败后强制重新加载图像 */
    function retryWallpaper() {
        if (wallpaper.length > 0)
            wallpaperReloadToken += 1
    }

    // ---- 交互色（浅色沿用图标的深青与亮青色阶，深色呼应启动页）----
    readonly property color primary: dark ? "#49C2C6" : "#076E7C"
    readonly property color primaryHover: dark ? "#72D4D6" : "#06616D"
    readonly property color primaryPressed: dark ? "#35A9AD" : "#05545F"
    //! 选中行 / 选中态着色
    readonly property color primaryTint: dark ? "#28545B" : "#E4F6F7"
    readonly property color textOnPrimary: dark ? "#102B33" : "#FFFFFF"
    // 浅色按钮直接使用图标深青 #087C8B；强调文字、悬停和按下逐级加深。
    readonly property color primaryButtonFill: dark ? primary : "#087C8B"
    readonly property color primaryButtonHoverFill: dark ? primaryHover : "#076E7C"
    readonly property color primaryButtonPressedFill: dark ? primaryPressed : "#065D69"

    // ---- 反馈色 ----
    readonly property color danger: dark ? "#F14C4C" : "#D32F2F"
    readonly property color dangerTint: dark ? "#3A2020" : "#FDECEA"
    //! 状态栏等处的警示文字
    readonly property color warning: dark ? "#FFA657" : "#EF6C00"

    // ---- 面与背景 ----
    //! 应用底色（中央区、ribbon 页背景）；同时作为无壁纸时的兜底背景，保持不透明
    readonly property color windowBackground: dark ? "#102B33" : "#F5FAFB"
    //! 面板 / 输入控件等前景面：壁纸可用时半透明透出背景；否则保持不透明
    readonly property color surface: wallpaperSurfacesActive ? (dark ? Qt.rgba(0.082, 0.204, 0.239, 0.72)
                                                                    : Qt.rgba(1.0, 1.0, 1.0, 0.72))
                                                             : (dark ? "#15343D" : "#FFFFFF")
    //! 工具栏、面板次级面：壁纸可用时半透明
    readonly property color surfaceAlt: wallpaperSurfacesActive ? (dark ? Qt.rgba(0.106, 0.251, 0.282, 0.62)
                                                                       : Qt.rgba(0.933, 0.965, 0.973, 0.62))
                                                                : (dark ? "#1B4048" : "#EEF6F8")
    //! 停靠面板标题面，与内容区拉开层次；壁纸可用时半透明
    readonly property color panelHeaderSurface: wallpaperSurfacesActive ? (dark ? Qt.rgba(0.125, 0.294, 0.325, 0.70)
                                                                               : Qt.rgba(0.894, 0.945, 0.957, 0.70))
                                                                        : (dark ? "#204B53" : "#E4F1F4")
    //! 菜单 / 弹窗前景面：不透明，保证弹窗内文字在壁纸上可读
    readonly property color menuSurface: dark ? "#15343D" : "#FFFFFF"
    //! 壁纸遮罩：叠在壁纸之上，使自定义背景随主题调色（浅色白亮化、深色青绿暗化）
    readonly property color wallpaperTint: dark ? Qt.rgba(0.063, 0.169, 0.200, 0.55)
                                                : Qt.rgba(1.0, 1.0, 1.0, 0.45)
    //! 列表项悬停着色
    readonly property color hoverOverlay: dark ? Qt.rgba(0.157, 0.329, 0.357, 0.50)
                                               : Qt.rgba(0.871, 0.949, 0.957, 0.50)
    //! 忙碌遮罩层
    readonly property color busyMaskOverlay: dark ? Qt.rgba(0.063, 0.169, 0.200, 0.55)
                                                 : Qt.rgba(0.93, 0.93, 0.93, 0.55)

    // ---- 边框 ----
    readonly property color border: dark ? "#35616A" : "#CDDFE4"
    readonly property color borderStrong: dark ? "#4A7880" : "#AAC7D0"

    // ---- 文本 ----
    readonly property color textPrimary: dark ? "#DDE8E9" : "#263E48"
    readonly property color textSecondary: dark ? "#B1C1C5" : "#536F7C"
    readonly property color textDisabled: dark ? "#789397" : "#8CA7B0"
    readonly property color textLink: primary

    // ---- 滚动条 ----
    readonly property color scrollBarIdle: dark ? "#35616A" : "#CBDEE3"
    readonly property color scrollBarHover: dark ? "#54838A" : "#A5C3CC"

    // ---- 停靠外壳专用 ----
    //! 停靠标题栏按钮图标
    readonly property color dockChromeIcon: dark ? "#B1C1C5" : "#536F7C"
    //! 停靠标题栏按钮悬停面
    readonly property color dockChromeHover: dark ? "#2B5961" : "#DFEFF3"
    //! 停靠分隔条悬停强调色
    readonly property color dockDividerHover: dark ? "#62B7BC" : "#86BAC6"
    //! 拖放落点强调色
    readonly property color dockDropAccent: dark ? "#49C2C6" : "#087C8B"
    //! 拖放落点填充
    readonly property color dockDropZoneFill: dark ? "#28545B" : "#E4F6F7"
    //! 拖放落点边框
    readonly property color dockDropZoneBorder: dark ? "#437B80" : "#8FC5CD"
    //! 拖放落点位置标记
    readonly property color dockDropZoneMarker: dark ? "#75A4A7" : "#79ACB9"
    //! 活跃落点前景/边框：与各主题的强调填充保持对比
    readonly property color dockDropZoneActiveFore: dark ? "#102B33" : "#FFFFFF"

    // ---- 调色板角色表 ----
    //! 普通控件调色板：主窗口、面板、应用级 QPalette；有壁纸时面色半透明
    readonly property var controlPalette: ({
            window: windowBackground,
            windowText: textPrimary,
            base: surface,
            alternateBase: surfaceAlt,
            text: textPrimary,
            button: surfaceAlt,
            buttonText: textPrimary,
            highlight: primary,
            highlightedText: textOnPrimary,
            placeholderText: textDisabled,
            mid: borderStrong,
            midlight: border,
            light: surface,
            dark: borderStrong,
            shadow: scrollBarHover,
            link: textLink,
            toolTipBase: menuSurface,
            toolTipText: textPrimary
        })
    //! 弹窗调色板：菜单与对话框共享；面保持不透明，保证壁纸上的可读性
    readonly property var popupPalette: ({
            window: menuSurface,
            windowText: textPrimary,
            base: menuSurface,
            alternateBase: menuSurface,
            text: textPrimary,
            button: menuSurface,
            buttonText: textPrimary,
            highlight: primary,
            highlightedText: textOnPrimary,
            placeholderText: textDisabled,
            mid: borderStrong,
            midlight: border,
            light: menuSurface,
            dark: borderStrong,
            shadow: scrollBarHover,
            link: textLink,
            toolTipBase: menuSurface,
            toolTipText: textPrimary
        })
    //! 停靠组件绘制色；与 dock QML 读取的键一一对应
    readonly property var dockColors: ({
            windowBackground: windowBackground,
            surface: surface,
            surfaceAlt: surfaceAlt,
            panelHeaderSurface: panelHeaderSurface,
            menuSurface: menuSurface,
            wallpaperTint: wallpaperTint,
            border: border,
            borderStrong: borderStrong,
            textPrimary: textPrimary,
            textDisabled: textDisabled,
            textOnPrimary: textOnPrimary,
            primary: primary,
            danger: danger,
            scrollBarHover: scrollBarHover,
            chromeIcon: dockChromeIcon,
            chromeHover: dockChromeHover,
            dividerHover: dockDividerHover,
            dropAccent: dockDropAccent,
            dropZoneFill: dockDropZoneFill,
            dropZoneBorder: dockDropZoneBorder,
            dropZoneMarker: dockDropZoneMarker,
            dropZoneActiveFore: dockDropZoneActiveFore
        })

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
