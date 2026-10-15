/**
 * @file TestWallpaperLibrary.cpp
 * @brief WallpaperLibrary 单元测试：导入校验、单张替换、移除与启动校验
 */

#include "WallpaperLibrary.h"

#include <catch2/catch_test_macros.hpp>
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSize>
#include <QStandardPaths>
#include <QString>
#include <QUrl>

namespace {

//! @brief 生成指定尺寸与颜色的测试图片并保存为 PNG
QString writePng(const QString& path, const QSize& size, const QColor& color)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QImage image(size, QImage::Format_RGB32);
    image.fill(color);
    image.save(path, "PNG");
    return path;
}

//! @brief 隔离真实用户数据：启用 Qt 测试模式并清空托管目录
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

TEST_CASE("WallpaperLibrary: reconcile falls back safely", "[Wallpaper]")
{
    prepareTestEnv();
    WallpaperLibrary library;

    const QString image = writePng(QDir::tempPath() + "/wallpaper_c.png", QSize(1920, 1080), Qt::magenta);
    const QString id = library.importWallpaper(QUrl::fromLocalFile(image));
    REQUIRE_FALSE(id.isEmpty());

    // id 失效但目录有残留：采纳唯一残留，不清空
    const QString adopted = library.reconcile(QStringLiteral("deadbeef.png"));
    REQUIRE(adopted == id);
    REQUIRE(storedCount() == 1);

    // 无选择：回退纯色并清理残留
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

    // 非法 id（含路径成分）返回空图，不越出托管目录
    REQUIRE(provider.requestImage(QStringLiteral("../evil.png"), nullptr, QSize(10, 10)).isNull());
}
