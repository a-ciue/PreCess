/**
 * @file TestWallpaperLibrary.cpp
 * @brief WallpaperLibrary 单元测试：导入校验、单张替换、移除与启动校验
 */

#include "WallpaperLibrary.h"

#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSize>
#include <QStandardPaths>
#include <QString>
#include <QUrl>
#include <catch2/catch_test_macros.hpp>

namespace {

QCoreApplication& testApplication()
{
    static int argc = 1;
    static char name[] = "TestWallpaperLibrary";
    static char* argv[] = { name, nullptr };
    static QCoreApplication app(argc, argv);
    return app;
}

//! @brief 生成指定尺寸与颜色的测试图片并保存为 PNG
QString writePng(const QString& path, const QSize& size, const QColor& color)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QImage image(size, QImage::Format_RGB32);
    image.fill(color);
    image.save(path, "PNG");
    return path;
}

//! @brief 把 qrc 测试图片复制到临时目录，供需要本地文件的导入入口使用
QString copyResource(const QString& resource, const QString& path)
{
    QFile source(resource);
    if (!source.open(QIODevice::ReadOnly))
        return {};
    QFile target(path);
    if (!target.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return {};
    if (target.write(source.readAll()) < 0)
        return {};
    return path;
}

//! @brief 隔离真实用户数据：启用 Qt 测试模式并清空托管目录
//!
//! 本二进制仅含壁纸相关用例（precess_add_test 每个目标为独立进程），
//! 故 setTestModeEnabled 的进程级副作用不会影响其它套件。
void prepareTestEnv()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("PreCessTest"));
    QCoreApplication::setApplicationName(QStringLiteral("PreCessTest"));
    QDir(WallpaperLibrary::userWallpaperDir()).removeRecursively();
}

//! @brief 托管目录内的文件数
int storedCount()
{
    QDir dir(WallpaperLibrary::userWallpaperDir());
    if (!dir.exists())
        return 0;
    return dir.entryList(QDir::Files | QDir::NoDotAndDotDot).size();
}

}

TEST_CASE("WallpaperLibrary: reject invalid sources", "[Wallpaper]")
{
    prepareTestEnv();
    WallpaperLibrary library;

    // 非本地 URL
    REQUIRE(library.importWallpaper(QUrl(QStringLiteral("http://example.com/a.png"))).isEmpty());

    // 不支持的后缀（内容为合法 PNG，但扩展名不在白名单）
    const QString png = writePng(QDir::tempPath() + "/wallpaper_src.png", QSize(320, 240), Qt::red);
    const QString gif = QDir::tempPath() + "/wallpaper_src.gif";
    QFile::remove(gif);
    REQUIRE(QFile::copy(png, gif));
    REQUIRE(library.importWallpaper(QUrl::fromLocalFile(gif)).isEmpty());
    QFile::remove(gif);

    REQUIRE(storedCount() == 0);
}

TEST_CASE("WallpaperLibrary: accepts any resolution and file size", "[Wallpaper]")
{
    prepareTestEnv();
    WallpaperLibrary library;

    // 小图不再被拒绝（自适应填充由显示层放大铺设）
    const QString small = writePng(QDir::tempPath() + "/wallpaper_small.png", QSize(64, 48), Qt::blue);
    REQUIRE_FALSE(library.importWallpaper(QUrl::fromLocalFile(small)).isEmpty());
    REQUIRE(storedCount() == 1);
}

TEST_CASE("WallpaperLibrary: async import completes and reports result", "[Wallpaper]")
{
    testApplication();
    prepareTestEnv();
    WallpaperLibrary library;

    const QString image = writePng(QDir::tempPath() + "/wallpaper_async.png", QSize(320, 240), Qt::yellow);
    QString imported_id;
    bool finished = false;
    bool failed = false;
    QObject::connect(&library, &WallpaperLibrary::importFinished, [&](const QString& id) {
        imported_id = id;
        finished = true;
    });
    QObject::connect(&library, &WallpaperLibrary::importFailed, [&] { failed = true; });

    library.importWallpaperAsync(QUrl::fromLocalFile(image));
    REQUIRE(library.importing());

    QElapsedTimer timer;
    timer.start();
    while (!finished && !failed && timer.elapsed() < 5000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);

    REQUIRE_FALSE(library.importing());
    REQUIRE(finished);
    REQUIRE_FALSE(failed);
    REQUIRE_FALSE(imported_id.isEmpty());
    REQUIRE(storedCount() == 1);
}

TEST_CASE("WallpaperLibrary: async import reports invalid source", "[Wallpaper]")
{
    testApplication();
    prepareTestEnv();
    WallpaperLibrary library;

    bool finished = false;
    bool failed = false;
    QObject::connect(&library, &WallpaperLibrary::importFinished, [&](const QString&) { finished = true; });
    QObject::connect(&library, &WallpaperLibrary::importFailed, [&] { failed = true; });

    library.importWallpaperAsync(QUrl(QStringLiteral("http://example.com/a.png")));

    QElapsedTimer timer;
    timer.start();
    while (!finished && !failed && timer.elapsed() < 5000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);

    REQUIRE_FALSE(library.importing());
    REQUIRE_FALSE(finished);
    REQUIRE(failed);
    REQUIRE(storedCount() == 0);
}

TEST_CASE("WallpaperLibrary: async import defers stale cleanup", "[Wallpaper]")
{
    testApplication();
    prepareTestEnv();
    WallpaperLibrary library;

    const QString first = writePng(QDir::tempPath() + "/wallpaper_cleanup_first.png", QSize(64, 48), Qt::red);
    REQUIRE_FALSE(library.importWallpaper(QUrl::fromLocalFile(first)).isEmpty());

    const QString second = writePng(QDir::tempPath() + "/wallpaper_cleanup_second.png", QSize(320, 240), Qt::blue);
    int count_in_signal = -1;
    bool finished = false;
    bool failed = false;
    QObject::connect(&library, &WallpaperLibrary::importFinished, [&](const QString&) {
        count_in_signal = storedCount();
        finished = true;
    });
    QObject::connect(&library, &WallpaperLibrary::importFailed, [&] { failed = true; });

    library.importWallpaperAsync(QUrl::fromLocalFile(second));

    QElapsedTimer timer;
    timer.start();
    while (!finished && !failed && timer.elapsed() < 5000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);

    REQUIRE_FALSE(library.importing());
    REQUIRE(finished);
    REQUIRE_FALSE(failed);
    // 回调时旧图仍存在，清理在其后执行
    REQUIRE(count_in_signal == 2);
    REQUIRE(storedCount() == 1);
}

TEST_CASE("WallpaperLibrary: reconcile skips while import is running", "[Wallpaper]")
{
    testApplication();
    prepareTestEnv();
    WallpaperLibrary library;

    const QString first = writePng(QDir::tempPath() + "/wallpaper_reconcile_first.png", QSize(64, 48), Qt::red);
    const QString saved_id = library.importWallpaper(QUrl::fromLocalFile(first));
    REQUIRE_FALSE(saved_id.isEmpty());

    const QString second = writePng(QDir::tempPath() + "/wallpaper_reconcile_second.png", QSize(320, 240), Qt::blue);
    bool finished = false;
    bool failed = false;
    QObject::connect(&library, &WallpaperLibrary::importFinished, [&](const QString&) { finished = true; });
    QObject::connect(&library, &WallpaperLibrary::importFailed, [&] { failed = true; });

    library.importWallpaperAsync(QUrl::fromLocalFile(second));
    REQUIRE(library.importing());
    // 导入期间不读目录、不清理文件，保持当前选择
    REQUIRE(library.reconcile(saved_id) == saved_id);

    QElapsedTimer timer;
    timer.start();
    while (!finished && !failed && timer.elapsed() < 5000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);

    REQUIRE_FALSE(library.importing());
    REQUIRE(finished);
    REQUIRE_FALSE(failed);
    REQUIRE(storedCount() == 1);
}

TEST_CASE("WallpaperLibrary: import copies and single-slot replaces", "[Wallpaper]")
{
    prepareTestEnv();
    WallpaperLibrary library;

    const QString first = writePng(QDir::tempPath() + "/wallpaper_a.png", QSize(1920, 1080), Qt::red);
    const QString first_id = library.importWallpaper(QUrl::fromLocalFile(first));
    REQUIRE_FALSE(first_id.isEmpty());
    REQUIRE(storedCount() == 1);

    // 同内容重复导入：幂等，id 不变、文件数不变
    REQUIRE(library.importWallpaper(QUrl::fromLocalFile(first)) == first_id);
    REQUIRE(storedCount() == 1);

    // 导入另一张不同内容：单张模式删除旧图，目录仍只有一张
    const QString second = writePng(QDir::tempPath() + "/wallpaper_b.png", QSize(1920, 1080), Qt::green);
    const QString second_id = library.importWallpaper(QUrl::fromLocalFile(second));
    REQUIRE_FALSE(second_id.isEmpty());
    REQUIRE(second_id != first_id);
    REQUIRE(storedCount() == 1);

    // reconcile 保留有效 id
    REQUIRE(library.reconcile(second_id) == second_id);

    // 移除：目录清空
    REQUIRE(library.removeWallpaper());
    REQUIRE(storedCount() == 0);
}

TEST_CASE("WallpaperLibrary: destruction before completion keeps previous file", "[Wallpaper]")
{
    testApplication();
    prepareTestEnv();

    const QString first = writePng(QDir::tempPath() + "/wallpaper_exit_first.png", QSize(64, 48), Qt::red);
    const QString second = writePng(QDir::tempPath() + "/wallpaper_exit_second.png", QSize(320, 240), Qt::blue);
    QString saved_id;
    {
        WallpaperLibrary library;
        saved_id = library.importWallpaper(QUrl::fromLocalFile(first));
        REQUIRE_FALSE(saved_id.isEmpty());
        library.importWallpaperAsync(QUrl::fromLocalFile(second));
        REQUIRE(library.importing());
    } // 析构等待 worker，但不处理排队的完成回调

    const QString saved_path = WallpaperLibrary::userWallpaperDir() + QLatin1Char('/') + saved_id;
    REQUIRE(QFile::exists(saved_path));

    // 模拟下次启动 reconcile：保留旧选择并清理可能残留的新图
    WallpaperLibrary recovered;
    REQUIRE(recovered.reconcile(saved_id) == saved_id);
    REQUIRE(storedCount() == 1);
    REQUIRE(QFile::exists(saved_path));
}

TEST_CASE("WallpaperLibrary: reconcile follows the saved choice", "[Wallpaper]")
{
    prepareTestEnv();
    WallpaperLibrary library;

    const QString image = writePng(QDir::tempPath() + "/wallpaper_c.png", QSize(1920, 1080), Qt::magenta);
    const QString id = library.importWallpaper(QUrl::fromLocalFile(image));
    REQUIRE_FALSE(id.isEmpty());

    // 有效 id：保留并清理其它残留
    REQUIRE(library.reconcile(id) == id);
    REQUIRE(storedCount() == 1);

    // id 失效：严格回退纯色，不按目录内容猜选其它图片
    REQUIRE(library.reconcile(QStringLiteral("deadbeef.png")).isEmpty());
    REQUIRE(storedCount() == 0);

    // 无选择：保持纯色
    REQUIRE(library.reconcile(QString()).isEmpty());
    REQUIRE(storedCount() == 0);
}

TEST_CASE("WallpaperProvider: adaptive scale and invalid id", "[Wallpaper]")
{
    prepareTestEnv();
    WallpaperLibrary library;

    const QString image = writePng(QDir::tempPath() + "/wallpaper_provider.png", QSize(400, 200), Qt::cyan);
    const QString id = library.importWallpaper(QUrl::fromLocalFile(image));
    REQUIRE_FALSE(id.isEmpty());

    WallpaperProvider provider;
    QSize out_size;
    const QImage scaled = provider.requestImage(id, &out_size, QSize(100, 100));
    REQUIRE_FALSE(scaled.isNull());
    // KeepAspectRatioByExpanding：两维均不小于目标尺寸，短边贴合
    REQUIRE(scaled.width() >= 100);
    REQUIRE(scaled.height() >= 100);
    REQUIRE(out_size == scaled.size());

    // 运行时缓存破除参数不参与文件名解析
    REQUIRE_FALSE(provider.requestImage(id + QStringLiteral("?v=1"), nullptr, QSize(100, 100)).isNull());

    // 非法 id（含路径成分）返回空图，不越出托管目录
    REQUIRE(provider.requestImage(QStringLiteral("../evil.png"), nullptr, QSize(10, 10)).isNull());
}

TEST_CASE("WallpaperProvider: headless fallback bounds targetless decode", "[Wallpaper]")
{
    prepareTestEnv();
    WallpaperLibrary library;

    const QString image = writePng(QDir::tempPath() + "/wallpaper_headless.png", QSize(4500, 3000), Qt::darkCyan);
    const QString id = library.importWallpaper(QUrl::fromLocalFile(image));
    REQUIRE_FALSE(id.isEmpty());

    // 强制无主屏分支：空 requestedSize 应回退 4K 目标而不是保留原图尺寸
    WallpaperProvider provider(WallpaperProvider::DecodeLimits {}, QSize(), false);
    const QImage decoded = provider.requestImage(id, nullptr, QSize());
    REQUIRE_FALSE(decoded.isNull());
    REQUIRE(decoded.size() != QSize(4500, 3000));
    REQUIRE(decoded.width() < 4500);
    REQUIRE(decoded.height() < 3000);
}

TEST_CASE("WallpaperProvider: never upscales small images", "[Wallpaper]")
{
    prepareTestEnv();
    WallpaperLibrary library;

    const QString image = writePng(QDir::tempPath() + "/wallpaper_small_provider.png", QSize(64, 48), Qt::darkBlue);
    const QString id = library.importWallpaper(QUrl::fromLocalFile(image));
    REQUIRE_FALSE(id.isEmpty());

    WallpaperProvider provider;
    const QImage decoded = provider.requestImage(id, nullptr, QSize(100, 100));
    REQUIRE_FALSE(decoded.isNull());
    // 小图保留原始像素，由显示层放大
    REQUIRE(decoded.size() == QSize(64, 48));
}

TEST_CASE("WallpaperProvider: thin images stay bounded", "[Wallpaper]")
{
    prepareTestEnv();
    WallpaperLibrary library;

    const QString image = writePng(QDir::tempPath() + "/wallpaper_thin_provider.png", QSize(4000, 10), Qt::darkRed);
    const QString id = library.importWallpaper(QUrl::fromLocalFile(image));
    REQUIRE_FALSE(id.isEmpty());

    WallpaperProvider provider;
    const QImage decoded = provider.requestImage(id, nullptr, QSize(192, 108));
    REQUIRE_FALSE(decoded.isNull());
    // 铺满所需的放大交给显示层，不生成 43200x108 的中间图
    REQUIRE(decoded.size() == QSize(4000, 10));
}

TEST_CASE("WallpaperProvider: scales formats without scaled decoding", "[Wallpaper]")
{
    prepareTestEnv();
    WallpaperLibrary library;

    const QString path = QDir::tempPath() + "/wallpaper_unscaled.bmp";
    QImage source(QSize(2000, 1000), QImage::Format_RGB32);
    source.fill(Qt::yellow);
    REQUIRE(source.save(path, "BMP"));
    const QString id = library.importWallpaper(QUrl::fromLocalFile(path));
    REQUIRE_FALSE(id.isEmpty());

    WallpaperProvider provider;
    QSize out_size;
    const QImage decoded = provider.requestImage(id, &out_size, QSize(192, 108));
    REQUIRE_FALSE(decoded.isNull());
    // BMP 等插件可能忽略 setScaledSize；read 后仍须缩到覆盖目标与上限之内
    REQUIRE(decoded.size() != QSize(2000, 1000));
    REQUIRE(decoded.width() <= 216);
    REQUIRE(decoded.height() <= 108);
    REQUIRE(out_size == decoded.size());
}

TEST_CASE("WallpaperProvider: caps output pixels", "[Wallpaper]")
{
    prepareTestEnv();
    WallpaperLibrary library;

    const QString image = writePng(QDir::tempPath() + "/wallpaper_large_provider.png", QSize(4000, 2000), Qt::darkGreen);
    const QString id = library.importWallpaper(QUrl::fromLocalFile(image));
    REQUIRE_FALSE(id.isEmpty());

    WallpaperProvider::DecodeLimits limits;
    limits.max_pixels = 10000;
    limits.max_dimension = 1000;
    WallpaperProvider provider(limits);

    const QImage decoded = provider.requestImage(id, nullptr, QSize(192, 108));
    REQUIRE_FALSE(decoded.isNull());
    REQUIRE(qint64(decoded.width()) * decoded.height() <= limits.max_pixels);
    REQUIRE(qMax(decoded.width(), decoded.height()) <= limits.max_dimension);
}

TEST_CASE("WallpaperProvider: applies EXIF orientation", "[Wallpaper]")
{
    prepareTestEnv();
    WallpaperLibrary library;

    const QString path = QDir::tempPath() + "/wallpaper_exif6.jpg";
    QFile::remove(path);
    REQUIRE_FALSE(copyResource(QStringLiteral(":/test-data/wallpaper_exif6.jpg"), path).isEmpty());
    const QString id = library.importWallpaper(QUrl::fromLocalFile(path));
    REQUIRE_FALSE(id.isEmpty());

    WallpaperProvider provider;
    QSize out_size;
    // 原始 400x200 + Orientation=6 显示为 200x400；缩放到 100x200
    const QImage decoded = provider.requestImage(id, &out_size, QSize(100, 100));
    REQUIRE_FALSE(decoded.isNull());
    REQUIRE(decoded.size() == QSize(100, 200));
    REQUIRE(out_size == decoded.size());
}
