#ifndef Q_MODEL_IO_SYSTEM_ADAPTOR_H
#define Q_MODEL_IO_SYSTEM_ADAPTOR_H

#include "Core.h"
#include <QUrl>
#include <QVariantList>
#include <QtQmlIntegration/qqmlintegration.h>

class QArgObject;
namespace systems::io {
class ModelIOSystem;
class QModelIOInfo;

/**
 * @brief 向Qml暴露的ModelIOSystem适配器，向界面暴露IO功能：读取模型、写出模型等各种功能操作
 */
class QModelIOSystemAdaptor : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("QModelIOSystemAdaptor is provided by C++")
    Q_PROPERTY(QStringList dialogNameFilters READ getDialogNameFilters NOTIFY dialogNameFiltersChanged)
public:
    QModelIOSystemAdaptor(ModelIOSystem& io_system);
    /**
     * @brief 按照给定文件类型，读取指定路径的文件
     * @param unique_name 文件类型唯一名称
     * @param url 文件路径
     * @param args 附加参数列表
     * @return 是否读取成功
     */
    Q_INVOKABLE bool read(const QString& unique_name, const QUrl& url, const QVariantList& args);
    /**
     * @brief 按照给定文件类型，写出指定模型到指定路径的文件
     * @param unique_name 文件类型唯一名称
     * @param model 模型索引
     * @param url 文件路径
     * @param args 附加参数列表
     * @return 是否写出成功
     */
    Q_INVOKABLE bool write(const QString& unique_name, Index model, const QUrl& url, const QVariantList& args);
    /**
     * @brief 推导导出文件对话框的预填文件：目录 + 按模型名与文件类型推导出的文件名
     * @param model_name 模型名，通常来自导入文件名（可带扩展名）
     * @param unique_name 文件类型唯一名称，取自文件对话框当前选中的过滤器名，允许未注册
     * @param folder 文件对话框当前目录
     * @return 供对话框预填的文件 URL，模型名或目录为空时返回空 URL（此时界面不预填）
     */
    Q_INVOKABLE QUrl suggestFileUrl(const QString& model_name, const QString& unique_name, const QUrl& folder) const;
    /**
     * @brief 按模型名与文件类型推导导出文件名（不含目录）
     *
     * 供不使用文件对话框的界面（如网页端导出弹窗）预填文件名，规则同 suggestFileUrl 的文件名部分。
     * @param model_name 模型名，通常来自导入文件名（可带扩展名）
     * @param unique_name 文件类型唯一名称，允许未注册
     * @return 建议文件名，模型名为空时返回空串
     */
    Q_INVOKABLE QString suggestFileName(const QString& model_name, const QString& unique_name) const;
    /**
     * @brief 写出前按文件类型校正目标文件的扩展名
     *
     * 对话框里的文件名不随用户改选的文件类型变化，写出前据此校正，避免内容与扩展名不符。
     * @param file 对话框选中的文件 URL
     * @param unique_name 文件类型唯一名称，允许未注册
     * @return 校正后的文件 URL，无需校正时原样返回
     */
    Q_INVOKABLE QUrl adaptFileExtension(const QUrl& file, const QString& unique_name) const;
    /**
     * @brief 获取所有支持的文件类型信息
     * @return 文件类型信息列表
     */
    Q_INVOKABLE QList<QModelIOInfo*> getModelIOInfo() const;
    /**
     * @brief 供文件对话框使用，获取所有支持的文件类型过滤器
     * @return 文件类型过滤器列表，如 ["Wavefront .obj file (*.obj)", "All files (*)"]
     */
    Q_INVOKABLE QStringList getDialogNameFilters() const;
    /**
     * @brief 供特定平台的文件选择器使用（如浏览器），获取所有支持的文件扩展名过滤器
     * @return 文件扩展名过滤器，如 ".obj,.mesh"
     */
    Q_INVOKABLE QString getDialogExtFilters() const;

signals:
    void dialogNameFiltersChanged();

private:
    /**
     * @brief 解析出实际使用的文件类型注册名：注册名优先，未指定或未知时按文件扩展名回退
     *
     * 界面文件对话框传入的是过滤器名，它未必等于注册名：用户选中默认项时是 "All files"，
     * 名字本身也可能与注册名不一致。读写两侧共用本函数，保证解析规则一致。
     * @param unique_name 界面传入的文件类型名，允许未注册
     * @param url 目标文件 URL，回退时按其扩展名匹配（导出时不要求文件已存在）
     * @return 已注册的文件类型唯一名称，无法解析时返回空串
     */
    QString resolveFileType(const QString& unique_name, const QUrl& url) const;

    /**
     * @brief 按文件扩展名解析出支持该文件类型的注册名
     * @param suffix 小写的、不含点的文件扩展名
     * @return 注册的文件类型唯一名称，无匹配时返回空串
     */
    QString resolveFileTypeBySuffix(const QString& suffix) const;

    ModelIOSystem* io_system_; //> 文件系统的引用
};
}

#endif