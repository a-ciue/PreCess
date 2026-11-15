/**
 * @file QStartupSplashWindow.cpp
 * @brief 原生栅格启动窗口，沿用 ZenithGrid 已确认设计稿
 */
#include "QStartupSplashWindow.h"

#include <QCloseEvent>
#include <QCursor>
#include <QEvent>
#include <QExposeEvent>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QPainter>
#include <QResizeEvent>
#include <QScreen>

#include <algorithm>
#include <spdlog/spdlog.h>

namespace {

constexpr qreal kDesignWidth = 960;
constexpr qreal kDesignHeight = 600;
constexpr qreal kAvailableScreenFraction = 0.9;
constexpr int kActivityTickMs = 40;
constexpr int kActivityCycleTicks = 120;
constexpr int kActivityInitialPhase = 26;
constexpr qreal kStatusTextWidth = 700;
constexpr qreal kActivityLeft = 52;
constexpr qreal kActivityTop = 575;
constexpr qreal kActivityWidth = 856;
constexpr qreal kActivityHeight = 3;
constexpr qreal kActivitySegmentWidth = 144;
constexpr qreal kIndicatorRadius = 6;
constexpr qreal kIndicatorStrokeWidth = 2;
const QPointF kIndicatorCenter(59, 548);
const QColor kBackgroundColor("#DDE7EA");
const QColor kPrimaryTextColor("#E1ECEF");
const QColor kSecondaryTextColor("#B8CED5");
const QColor kStatusTextColor("#536F7C");
const QColor kIndicatorColor("#087C8B");
const QColor kActivityColor("#159FAC");

} // namespace

QStartupSplashWindow::QStartupSplashWindow(const QString& version, const QString& background_path)
    : backing_store_(this)
    , background_(background_path)
    , version_(version)
    , activity_phase_(kActivityInitialPhase)
{
    // 封面早于应用日志面板初始化；先通过 spdlog 默认 sink 记录资源告警。
    if (background_.isNull())
        spdlog::warn("startup: cannot load splash background '{}'; using a solid background", background_path.toStdString());

    setObjectName(QStringLiteral("startupSplash"));
    setTitle(tr("ZenithGrid — 正在启动"));
    setSurfaceType(QSurface::RasterSurface);
    setFlags(Qt::SplashScreen | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setModality(Qt::ApplicationModal);

    auto* target_screen = QGuiApplication::screenAt(QCursor::pos());
    if (!target_screen)
        target_screen = QGuiApplication::primaryScreen();
    if (target_screen) {
        setScreen(target_screen);
        const QRect available = target_screen->availableGeometry();
        const qreal scale = std::min({ 1.0, available.width() * kAvailableScreenFraction / kDesignWidth,
            available.height() * kAvailableScreenFraction / kDesignHeight });
        const QSize splash_size(std::max(1, qRound(kDesignWidth * scale)), std::max(1, qRound(kDesignHeight * scale)));
        setGeometry(QRect(available.center() - QPoint(splash_size.width() / 2, splash_size.height() / 2), splash_size));
    } else {
        resize(qRound(kDesignWidth), qRound(kDesignHeight));
    }

    activity_timer_.setInterval(kActivityTickMs);
    connect(&activity_timer_, &QTimer::timeout, this, [this]() {
        activity_phase_ = (activity_phase_ + 1) % kActivityCycleTicks;
        renderNow();
    });
    connect(this, &QWindow::visibleChanged, this, [this](bool visible) {
        if (visible)
            activity_timer_.start();
        else
            activity_timer_.stop();
    });
}

bool QStartupSplashWindow::event(QEvent* event)
{
    if (event->type() == QEvent::UpdateRequest)
        renderNow();
    return QWindow::event(event);
}

void QStartupSplashWindow::exposeEvent(QExposeEvent* event)
{
    QWindow::exposeEvent(event);
    renderNow();
}

void QStartupSplashWindow::resizeEvent(QResizeEvent* event)
{
    QWindow::resizeEvent(event);
    renderNow();
}

void QStartupSplashWindow::closeEvent(QCloseEvent* event)
{
    QWindow::closeEvent(event);
    emit onCancelled();
}

void QStartupSplashWindow::renderNow()
{
    if (!isExposed() || !isVisible() || size().isEmpty())
        return;
    backing_store_.resize(size());
    const QRegion dirty(QRect(QPoint(), size()));
    backing_store_.beginPaint(dirty);
    {
        QPainter painter(backing_store_.paintDevice());
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.scale(width() / kDesignWidth, height() / kDesignHeight);
        paintContents(painter);
    }
    backing_store_.endPaint();
    backing_store_.flush(dirty);
    if (!first_frame_) {
        first_frame_ = true;
        emit onFirstFrame();
    }
}

void QStartupSplashWindow::paintContents(QPainter& painter) const
{
    const QRectF background_rect(0, 0, kDesignWidth, kDesignHeight);
    painter.fillRect(background_rect, kBackgroundColor);
    if (!background_.isNull())
        painter.drawImage(background_rect, background_);
    auto text = [&painter](const QString& value, qreal x, qreal y, int size, const QColor& color,
                    QFont::Weight weight = QFont::Normal, qreal spacing = 0.0, bool right = false) {
        QFont font = QGuiApplication::font();
        font.setFamilies({ QStringLiteral("Segoe UI"), QStringLiteral("Microsoft YaHei UI"), QStringLiteral("sans-serif") });
        font.setPixelSize(size);
        font.setWeight(weight);
        font.setLetterSpacing(QFont::AbsoluteSpacing, spacing);
        painter.setFont(font);
        painter.setPen(color);
        if (right)
            x -= QFontMetricsF(font).horizontalAdvance(value);
        painter.drawText(QPointF(x, y), value);
    };

    // 缺少深浅分区的设计背景时，标题采用适合纯色底的文字颜色。
    const QColor& heading_color = background_.isNull() ? kStatusTextColor : kPrimaryTextColor;
    const QColor& secondary_color = background_.isNull() ? kStatusTextColor : kSecondaryTextColor;
    // 下列文字锚点及字号是同一设计画布上的排版数据，随画布统一缩放。
    text(tr("几何 · 网格 · 工程"), 88, 54, 14, secondary_color, QFont::Normal, 1);
    text(QStringLiteral("GEOMETRY"), 50, 131, 44, heading_color, QFont::Normal, 2);
    text(QStringLiteral("& MESH"), 50, 185, 44, secondary_color, QFont::Light, 2);
    text(tr("几何与网格前处理"), 168, 485, 16, kStatusTextColor, QFont::Normal, 1);
    text(tr("版本"), 908, 403, 14, kStatusTextColor, QFont::Normal, 1, true);
    text(QStringLiteral("v%1").arg(version_), 910, 442, 30, kStatusTextColor, QFont::Normal, 0.5, true);

    QFont status_font = QGuiApplication::font();
    status_font.setPixelSize(16);
    painter.setFont(status_font);
    painter.setPen(kStatusTextColor);
    const QString elided = QFontMetricsF(status_font).elidedText(tr("正在启动…"), Qt::ElideMiddle, kStatusTextWidth);
    painter.drawText(QPointF(80, 554), elided);
    painter.setPen(QPen(kPrimaryTextColor, kIndicatorStrokeWidth));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(kIndicatorCenter, kIndicatorRadius, kIndicatorRadius);
    painter.setPen(QPen(kIndicatorColor, kIndicatorStrokeWidth, Qt::SolidLine, Qt::RoundCap));
    const QRectF indicator_rect(kIndicatorCenter - QPointF(kIndicatorRadius, kIndicatorRadius),
        QSizeF(kIndicatorRadius * 2, kIndicatorRadius * 2));
    constexpr int kAngleUnitsPerDegree = 16; // QPainter 的角度单位为 1/16 度。
    const int rotation = activity_phase_ * 360 / kActivityCycleTicks;
    painter.drawArc(indicator_rect, (90 - rotation) * kAngleUnitsPerDegree, -270 * kAngleUnitsPerDegree);
    painter.setPen(Qt::NoPen);
    painter.setBrush(kPrimaryTextColor);
    const qreal corner_radius = kActivityHeight / 2;
    painter.drawRoundedRect(QRectF(kActivityLeft, kActivityTop, kActivityWidth, kActivityHeight), corner_radius, corner_radius);
    painter.setBrush(kActivityColor);
    // 中间短段往返，明确表示活动而非固定百分比。
    constexpr int kHalfCycleTicks = kActivityCycleTicks / 2;
    const int phase = std::min(activity_phase_, kActivityCycleTicks - activity_phase_);
    const qreal segment_left = kActivityLeft + phase * ((kActivityWidth - kActivitySegmentWidth) / kHalfCycleTicks);
    painter.drawRoundedRect(QRectF(segment_left, kActivityTop, kActivitySegmentWidth, kActivityHeight), corner_radius, corner_radius);
}
