#include "QWasmBridge.h"
#include <cstdlib>
#include <QFile>
#include <QStringList>
#include <emscripten.h>
#include <spdlog/spdlog.h>

namespace {
constexpr char TEMP_DIR[] = "/tmp";

EM_JS(void, open_file_picker, (const char* accept, bool multiple, const char* dir), {
    const input = document.createElement('input');
    input.type = 'file';
    const accept_value = UTF8ToString(accept);
    if (accept_value) input.accept = accept_value;
    input.multiple = !!multiple;
    input.addEventListener('change', async () => {
        const dir_path = UTF8ToString(dir);
        const files = Array.from(input.files);
        for (const file of files) {
            const data = new Uint8Array(await file.arrayBuffer());
            FS.writeFile(dir_path + '/' + file.name, data);
        }
        if (files.length > 0 && Module._on_file_uploaded) {
            const names = stringToNewUTF8(files.map(file => file.name).join('\n'));
            Module._on_file_uploaded(names);
        }
    });
    input.click();
});

EM_JS(void, download_file, (const char* file_name, const char* memfs_path), {
    const data = FS.readFile(UTF8ToString(memfs_path));
    const blob = new Blob([data], { type: 'application/octet-stream' });
    const link = document.createElement('a');
    link.href = URL.createObjectURL(blob);
    link.download = UTF8ToString(file_name);
    document.body.appendChild(link);
    link.click();
    link.remove();
    setTimeout(function () {
        URL.revokeObjectURL(link.href);
    }, 5000);
});
} // namespace

extern "C" EMSCRIPTEN_KEEPALIVE void on_file_uploaded(const char* names)
{
    if (names == nullptr)
        return;
    const QStringList name_list =
        QString::fromUtf8(names).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    // names 由 JS 侧 stringToNewUTF8 分配，调用 free 释放是安全的
    std::free(const_cast<char*>(names));
    if (name_list.isEmpty())
        return;

    QWasmBridge* bridge = QWasmBridge::instance();
    if (bridge == nullptr)
        return;

    QStringList paths;
    paths.reserve(name_list.size());
    for (const QString& name : name_list)
        paths << QString::fromLatin1(TEMP_DIR) + QLatin1Char('/') + name;

    spdlog::info("uploaded {} file(s)", paths.size());
    emit bridge->filesImported(paths);

    for (const QString& path : paths)
        QFile::remove(path);
}

QWasmBridge* QWasmBridge::instance_ = nullptr;

QWasmBridge* QWasmBridge::instance()
{
    return instance_;
}

QWasmBridge::QWasmBridge(QObject* parent)
    : QObject(parent)
{
    if (instance_ != nullptr) {
        return;
    }
    instance_ = this;
}

QWasmBridge::~QWasmBridge()
{
    if (instance_ == this)
        instance_ = nullptr;
}

void QWasmBridge::pickFile(const QString& accept, bool multiple)
{
    open_file_picker(accept.toUtf8().constData(), multiple, TEMP_DIR);
}

bool QWasmBridge::downloadFile(const QString& name, const QString& path)
{
    if (!QFile::exists(path)) {
        spdlog::error("file to download does not exist: {}", path.toStdString());
        return false;
    }
    spdlog::info("downloading {} -> {}", path.toStdString(), name.toStdString());
    download_file(name.toUtf8().constData(), path.toUtf8().constData());
    return true;
}
