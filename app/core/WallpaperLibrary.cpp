/**
 * @file WallpaperLibrary.cpp
 * @brief 壁纸库实现：图像提供者与导入/移除/启动校验
 */

#include "WallpaperLibrary.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImageIOHandler>
#include <QImageReader>
#include <QRunnable>
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

//! @brief 删除托管目录内除 keep 之外的壁纸文件，返回是否全部删除成功
bool removeOtherFiles(const QString& keep)
{
    QDir dir(WallpaperLibrary::userWallpaperDir());
    bool ok = true;
    for (const QString& name : wallpaperFiles()) {
        if (name == keep)
            continue;
        if (!dir.remove(name)) {
            spdlog::warn("wallpaper: failed to remove stale file: {}", dir.filePath(name).toStdString());
            ok = false;
        }
    }
    return ok;
}

//! @brief 按目标覆盖尺寸与输出上限计算解码尺寸；绝不放大
QSize constrainedImageSize(const QSize& source, const QSize& target,
    const WallpaperProvider::DecodeLimits& limits)
{
    if (!source.isValid() || source.isEmpty())
        return {};

    qreal scale = 1.0;
    if (target.isValid() && !target.isEmpty()) {
        scale = qMin(qreal(1), qMax(qreal(target.width()) / source.width(), qreal(target.height()) / source.height()));
    }

    if (limits.max_pixels > 0) {
        const qreal pixels = qreal(source.width()) * source.height() * scale * scale;
        if (pixels > limits.max_pixels)
            scale *= qSqrt(qreal(limits.max_pixels) / pixels);
    }

    if (limits.max_dimension > 0) {
        const qreal longest = qMax(source.width(), source.height()) * scale;
        if (longest > limits.max_dimension)
            scale *= qreal(limits.max_dimension) / longest;
    }

    // 向下取整，保证取整后仍不越过像素/边长上限
    return QSize(qMax(1, qFloor(source.width() * scale)),
        qMax(1, qFloor(source.height() * scale)));
}

//! @brief 最终兜底：确保插件忽略 setScaledSize 时仍不放大、不超限
QImage fitDecodedImage(QImage image, const QSize& target,
    const WallpaperProvider::DecodeLimits& limits)
{
    if (image.isNull())
        return image;

    const QSize constrained = constrainedImageSize(image.size(), target, limits);
    if (!constrained.isValid() || constrained == image.size())
        return image;
    return image.scaled(constrained, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

} // namespace

WallpaperProvider::WallpaperProvider(DecodeLimits limits)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , limits_(limits)
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
    if (!reader.canRead()) {
        spdlog::debug("wallpaper: cannot read image: {}", path.toStdString());
        return {};
    }

    // 按 EXIF 方向显示；尺寸计算使用旋转后的宽高
    reader.setAutoTransform(true);
    const QSize raw_size = reader.size();
    const bool swaps_axes = (reader.transformation() & QImageIOHandler::TransformationRotate90) != 0;
    const QSize oriented_size = swaps_axes ? raw_size.transposed() : raw_size;

    // 目标尺寸：优先 QML 请求尺寸，否则回退主屏解码上限
    const QSize target = (requestedSize.isValid() && !requestedSize.isEmpty()) ? requestedSize : decode_limit_;

    // 先按目标与上限设置解码尺寸，避免插件先全量解码再缩放
    if (oriented_size.isValid() && !oriented_size.isEmpty()) {
        const QSize oriented_scaled = constrainedImageSize(oriented_size, target, limits_);
        const QSize raw_scaled = swaps_axes ? oriented_scaled.transposed() : oriented_scaled;
        reader.setScaledSize(raw_scaled);
    }

    // 部分格式插件（如 BMP）会忽略 setScaledSize，read 后统一由 fitDecodedImage 兜底
    QImage image = fitDecodedImage(reader.read(), target, limits_);
    if (image.isNull()) {
        spdlog::debug("wallpaper: failed to decode {}: {}", path.toStdString(),
            reader.errorString().toStdString());
        return {};
    }

    spdlog::debug("wallpaper: decoded {}x{} from {}x{} (swap axes: {}, target {}x{})",
        image.width(), image.height(), raw_size.width(), raw_size.height(), swaps_axes, target.width(),
        target.height());
    if (size)
        *size = image.size();
    return image;
}

WallpaperLibrary::WallpaperLibrary(QObject* parent)
    : QObject(parent)
{
    // 单槽：同一时间只允许一个导入任务，避免单张模式互相删除结果
    import_pool_.setMaxThreadCount(1);
}

WallpaperLibrary::~WallpaperLibrary()
{
    // 工作线程可能仍在使用壁纸目录；析构前等待，避免对象销毁后回投
    import_pool_.waitForDone();
}

QString WallpaperLibrary::userWallpaperDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/wallpapers");
}

namespace {

//! @brief 同步导入实现；不触碰 WallpaperLibrary 状态，可在工作线程调用
QString importWallpaperImpl(const QUrl& source_url)
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
    QDir dir(WallpaperLibrary::userWallpaperDir());
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

}

QString WallpaperLibrary::importWallpaper(const QUrl& source_url)
{
    if (importing_)
        return {};
    return importWallpaperImpl(source_url);
}

void WallpaperLibrary::importWallpaperAsync(const QUrl& source_url)
{
    if (importing_)
        return;

    setImporting(true);
    import_pool_.start(QRunnable::create([this, source_url] {
        const QString id = importWallpaperImpl(source_url);
        QMetaObject::invokeMethod(this, [this, id] { finishImport(id); }, Qt::QueuedConnection);
    }));
}

void WallpaperLibrary::finishImport(const QString& id)
{
    setImporting(false);
    if (id.isEmpty())
        emit importFailed();
    else
        emit importFinished(id);
}

void WallpaperLibrary::setImporting(bool importing)
{
    if (importing_ == importing)
        return;
    importing_ = importing;
    emit importingChanged();
}

bool WallpaperLibrary::removeWallpaper()
{
    if (importing_)
        return false;
    return removeOtherFiles(QString());
}

QString WallpaperLibrary::reconcile(const QString& id)
{
    const QStringList files = wallpaperFiles();
    if (!id.isEmpty() && files.contains(id)) {
        removeOtherFiles(id);
        return id;
    }
    // 选择为空或失效：严格回退纯色，不按目录内容推断用户选择；同时清理残留
    removeOtherFiles(QString());
    return {};
}
