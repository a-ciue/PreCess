#ifndef Q_WASM_BRIDGE_H
#define Q_WASM_BRIDGE_H

#include <QObject>

class QWasmBridge : public QObject {
    Q_OBJECT

public:
    explicit QWasmBridge(QObject* parent = nullptr);
    ~QWasmBridge() override;

    Q_INVOKABLE void pickFile(const QString& accept, bool multiple);
    Q_INVOKABLE bool downloadFile(const QString& name, const QString& path);

    static QWasmBridge* instance();

signals:
    void filesImported(const QStringList& paths);

private:
    static QWasmBridge* instance_;
};

#endif // Q_WASM_BRIDGE_H
