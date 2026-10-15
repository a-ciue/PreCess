/**
 * @file WallpaperLibrary.cpp
 * @brief 壁纸库实现：图像提供者与导入/移除/启动校验
 */

#include "WallpaperLibrary.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImageReader>
#include <QScreen>
#include <QStandardPaths>
#include <QStringList>
#include <QtMath>
#include <spdlog/spdlog.h>

namespace {

//! 允许的图片后缀（小写）
const QStringList kAllowedSuffixes { QStringLiteral("jpg"), QStringLiteral("jpeg"),
    QStringLiteral("png"), QStringLiteral("webp"), QStringLiteral("bmp") };

//! @brief 判断 id 是否为合法的托管文件名（不含路径成分）
bool isSafeId(const QString& id)
{
    return !id.isEmpty() && !id.contains(QLatin1Char('/')) && !id.contains(QLatin1Char('\\'))
        && id != QLatin1String(".") && id != QLatin1String("..");
}

//! @brief 计算文件内容的 SHA256（分块，避免整文件读入内存）
QString fileContentHash(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file))
        return {};
    return QString::fromLatin1(hash.result().toHex());
}

//! @brief 列出托管目录内的文件（不含 . 与 ..）
QStringList wallpaperFiles()
{
    QDir dir(WallpaperLibrary::userWallpaperDir());
    if (!dir.exists())
        return {};
    return dir.entryList(QDir::Files | QDir::NoDotAndDotDot);
}

//! @brief 删除托管目录内除 keep 之外的壁纸文件
void removeOtherFiles(const QString& keep)
{
    QDir dir(WallpaperLibrary::userWallpaperDir());
    for (const QString& name : wallpaperFiles()) {
        if (name != keep && !dir.remove(name))
            spdlog::warn("wallpaper: failed to remove stale file: {}", dir.filePath(name).toStdString());
    }
}

}

WallpaperProvider::WallpaperProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
    // 构造发生在 GUI 线程：在此一次性求值主屏解码上限（含设备像素比）。requestImage
    // 在 Image.asynchronous 下可能由异步加载线程调用，故只读缓存成员，避免在请求路径
    // 重复访问 QScreen（QScreen 非线程安全）
    if (auto* screen = QGuiApplication::primaryScreen()) {
        const QSize logical = screen->geometry().size();
        const qreal dpr = screen->devicePixelRatio();
        decode_limit_ = QSize(qCeil(logical.width() * dpr), qCeil(logical.height() * dpr));
    }
}

QImage WallpaperProvider::requestImage(const QString& id, QSize* size, const QSize& requestedSize)
{
    if (size)
        *size = QSize();
    if (!isSafeId(id))
        return {};

    const QString path = WallpaperLibrary::userWallpaperDir() + QLatin1Char('/') + id;
    QImageReader reader(path);
    if (!reader.canRead())
        return {};

    // 目标尺寸：优先 QML 请求尺寸，否则回退主屏解码上限；不限制导入图片的原始分辨率
    const QSize target = (requestedSize.isValid() && !requestedSize.isEmpty()) ? requestedSize : decode_limit_;

    QImage image;
    const QSize source_size = reader.size();
    if (!target.isEmpty() && source_size.isValid() && !source_size.isEmpty()) {
        // 按 KeepAspectRatioByExpanding 计算缩放尺寸，边解码边缩放（支持缩放的插件可省去全量解码）
        const qreal ratio = qMax(qreal(target.width()) / source_size.width(),
            qreal(target.height()) / source_size.height());
        reader.setScaledSize(QSize(qCeil(source_size.width() * ratio), qCeil(source_size.height() * ratio)));
        image = reader.read();
    } else {
        image = reader.read();
        if (!image.isNull() && !target.isEmpty())
            image = image.scaled(target, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    }

    if (image.isNull())
        return {};
    if (size)
        *size = image.size();
    return image;
}

WallpaperLibrary::WallpaperLibrary(QObject* parent)
    : QObject(parent)
{
}

QString WallpaperLibrary::userWallpaperDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/wallpapers");
}

QString WallpaperLibrary::importWallpaper(const QUrl& source_url)
{
    if (!source_url.isLocalFile()) {
        spdlog::warn("wallpaper: rejected non-local source: {}", source_url.toString().toStdString());
        return {};
    }

    const QString source_path = source_url.toLocalFile();
    const QFileInfo info(source_path);
    const QString suffix = info.suffix().toLower();
    if (!kAllowedSuffixes.contains(suffix)) {
        spdlog::debug("wallpaper: rejected unsupported suffix: {}", source_path.toStdString());
        return {};
    }

    // 校验可解码（不限制分辨率与图片大小）；再做一次小尺寸解码，拒绝
    // “能识别格式但内容损坏”的文件。支持缩放解码的格式不会全量解码。
    QImageReader reader(source_path);
    if (!reader.canRead()) {
        spdlog::debug("wallpaper: rejected unreadable file: {}", source_path.toStdString());
        return {};
    }
    const QSize original_size = reader.size();
    if (original_size.isValid() && !original_size.isEmpty()) {
        const qreal ratio = qMin(qreal(64) / original_size.width(), qreal(64) / original_size.height());
        if (ratio < 1.0)
            reader.setScaledSize(QSize(qMax(1, qCeil(original_size.width() * ratio)),
                qMax(1, qCeil(original_size.height() * ratio))));
    }
    if (reader.read().isNull()) {
        spdlog::debug("wallpaper: rejected undecodable file: {}", source_path.toStdString());
        return {};
    }

    const QString content_hash = fileContentHash(source_path);
    if (content_hash.isEmpty()) {
        spdlog::error("wallpaper: failed to hash source: {}", source_path.toStdString());
        return {};
    }

    const QString store_name = content_hash.left(16) + QLatin1Char('.') + suffix;
    QDir dir(userWallpaperDir());
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        spdlog::error("wallpaper: failed to create directory: {}", dir.absolutePath().toStdString());
        return {};
    }

    const QString target_path = dir.filePath(store_name);
    // 同内容重复导入：目标已存在则跳过复制，仍返回同一 id
    if (!QFile::exists(target_path) && !QFile::copy(source_path, target_path)) {
        spdlog::error("wallpaper: failed to copy {} -> {}", source_path.toStdString(),
            target_path.toStdString());
        return {};
    }

    // 单张模式：删除除新图外的其它文件
    removeOtherFiles(store_name);
    return store_name;
}

bool WallpaperLibrary::removeWallpaper()
{
    QDir dir(userWallpaperDir());
    if (!dir.exists())
        return true;

    bool ok = true;
    for (const QString& name : wallpaperFiles()) {
        if (!dir.remove(name)) {
            spdlog::warn("wallpaper: failed to remove file: {}", dir.filePath(name).toStdString());
            ok = false;
        }
    }
    return ok;
}

QString WallpaperLibrary::reconcile(const QString& id)
{
    const QStringList files = wallpaperFiles();
    if (!id.isEmpty() && files.contains(id)) {
        removeOtherFiles(id);
        return id;
    }
    // 选择失效但目录仍有残留：采纳最新一张，避免唯一一张被误清
    if (!id.isEmpty() && !files.isEmpty()) {
        QDir dir(userWallpaperDir());
        QString newest;
        QDateTime newest_time;
        for (const QString& name : files) {
            const QDateTime modified = QFileInfo(dir.filePath(name)).lastModified();
            if (newest.isEmpty() || modified > newest_time) {
                newest = name;
                newest_time = modified;
            }
        }
        removeOtherFiles(newest);
        return newest;
    }
    // 无选择：清理残留，回退纯色
    if (id.isEmpty())
        removeOtherFiles(QString());
    return {};
}
