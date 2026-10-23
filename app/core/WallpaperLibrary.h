/**
 * @file WallpaperLibrary.h
 * @brief 自定义壁纸库：托管用户导入的壁纸并把文件名解析为图像
 *
 * 导入的图片复制到 AppDataLocation/wallpapers/，以内容哈希命名（单张模式：目录内
 * 始终至多一张）。QSettings 只保存文件名（id），运行时经 image://wallpaper/ 解析到
 * 托管目录——原文件被移动/改名后仍可用，也不受托管目录位置变化影响。
 */

#pragma once

#include <QImage>
#include <QObject>
#include <QQuickImageProvider>
#include <QSize>
#include <QString>
#include <QThreadPool>
#include <QUrl>
#include <qqmlintegration.h>

/**
 * @brief image://wallpaper/ 图像提供者
 *
 * id 为托管目录内的文件名；非法 id 或缺失文件返回空图像，QML 侧回退纯色背景。
 * 解码遵循“不放大、限制输出规模、按 EXIF 方向显示”的规则：优先采用 QML 请求
 * 尺寸，否则以主屏分辨率（含设备像素比）为解码上限；小图保留原始像素由显示层
 * 放大，超大图按像素总量与最长边上限降低解码分辨率。
 *
 * QML 引擎会按 image:// URL 缓存 provider 结果；当前请求不设置 sourceSize，
 * 不随布局变化重复解码，因此不需要额外的 provider 内部缓存。
 */
class WallpaperProvider : public QQuickImageProvider {
public:
    //! @brief 解码输出上限；测试可注入更小值验证降采样路径
    struct DecodeLimits {
        qint64 max_pixels = 16'000'000;
        int max_dimension = 8192;
    };

    explicit WallpaperProvider(DecodeLimits limits = {});

    /** @brief 按 EXIF 方向读取并缩放到目标尺寸（KeepAspectRatioByExpanding，禁止放大） */
    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

private:
    //! @brief 无 QML 请求尺寸时的解码上限（主屏分辨率 × 设备像素比）
    QSize decode_limit_;
    //! @brief 输出规模上限
    DecodeLimits limits_;
};

/**
 * @brief 壁纸库 QML 单例：导入/移除/启动校验用户壁纸
 */
class WallpaperLibrary : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Wallpaper)
    QML_SINGLETON
    Q_PROPERTY(bool importing READ importing NOTIFY importingChanged)

public:
    explicit WallpaperLibrary(QObject* parent = nullptr);
    ~WallpaperLibrary() override;

    /**
     * @brief 同步导入壁纸：校验后复制到托管目录并删除旧图
     * @param source_url 本地图片 URL
     * @return 成功返回托管文件名（id），失败返回空串
     */
    Q_INVOKABLE QString importWallpaper(const QUrl& source_url);

    /**
     * @brief 异步导入壁纸（工作线程校验/哈希/复制，GUI 线程回传结果）
     * @param source_url 本地图片 URL
     */
    Q_INVOKABLE void importWallpaperAsync(const QUrl& source_url);

    //! @brief 是否有异步导入正在进行
    bool importing() const { return importing_; }

    /** @brief 删除托管目录内的壁纸文件 */
    Q_INVOKABLE bool removeWallpaper();

    /**
     * @brief 启动校验：id 有效且文件存在则保留并清理其它残留；否则清理残留并回退纯色
     * @param id 当前持久化的壁纸 id
     * @return 可用的壁纸 id，或空串（纯色默认）
     */
    Q_INVOKABLE QString reconcile(const QString& id);

    /** @brief 托管目录（AppDataLocation/wallpapers） */
    static QString userWallpaperDir();

signals:
    void importingChanged();
    void importFinished(const QString& id);
    void importFailed();

private:
    //! @brief 工作线程完成后在 GUI 线程收尾
    void finishImport(const QString& id);
    void setImporting(bool importing);

    QThreadPool import_pool_;
    bool importing_ = false;
};
