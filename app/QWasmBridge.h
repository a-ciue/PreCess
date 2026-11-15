#ifndef Q_WASM_BRIDGE_H
#define Q_WASM_BRIDGE_H

#include <QObject>
#include <QStringList>

class QWasmBridge : public QObject {
    Q_OBJECT

public:
    explicit QWasmBridge(QObject* parent = nullptr);
    ~QWasmBridge() override;

    Q_INVOKABLE void pickFile(const QString& accept, bool multiple);
    Q_INVOKABLE bool downloadFile(const QString& name, const QString& path);
    //! @brief 文件操作结束后释放上传文件及其空临时目录。
    Q_INVOKABLE void releaseFiles(const QStringList& paths);

    static QWasmBridge* instance();

signals:
    void filesImported(const QStringList& paths);

private:
    static QWasmBridge* instance_;
};

#endif // Q_WASM_BRIDGE_H
