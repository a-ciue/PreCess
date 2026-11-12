#include "QWasmBridge.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>
#include <cstdlib>
#include <emscripten.h>
#include <spdlog/spdlog.h>

namespace {
constexpr char TEMP_DIR[] = "/tmp";

// clang-format off
EM_JS(void, open_file_picker, (const char* accept, bool multiple, const char* dir), {
    const input = document.createElement('input');
    input.type = 'file';
    const accept_value = UTF8ToString(accept);
    if (accept_value) input.accept = accept_value;
    input.multiple = !!multiple;
    const dir_path = UTF8ToString(dir);
    input.addEventListener('change', async () => {
        const files = Array.from(input.files);
        if (files.length === 0) return;
        FS.mkdir(dir_path);
        for (const file of files) {
            const data = new Uint8Array(await file.arrayBuffer());
            FS.writeFile(dir_path + '/' + file.name, data);
        }
        if (Module._on_file_uploaded) {
            const names = stringToNewUTF8(files.map(file => dir_path + '/' + file.name).join('\n'));
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
// clang-format on
} // namespace

extern "C" EMSCRIPTEN_KEEPALIVE void on_file_uploaded(const char* names)
{
    if (names == nullptr)
        return;
    const QStringList paths = QString::fromUtf8(names).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    // names 由 JS 侧 stringToNewUTF8 分配，调用 free 释放是安全的
    std::free(const_cast<char*>(names));
    if (paths.isEmpty())
        return;

    QWasmBridge* bridge = QWasmBridge::instance();
    if (bridge == nullptr)
        return;

    spdlog::info("uploaded {} file(s)", paths.size());
    emit bridge->filesImported(paths);
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
    const auto directory = QString::fromLatin1(TEMP_DIR) + "/precess-upload-" + QUuid::createUuid().toString(QUuid::Id128);
    open_file_picker(accept.toUtf8().constData(), multiple, directory.toUtf8().constData());
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

void QWasmBridge::releaseFiles(const QStringList& paths)
{
    for (const auto& path : paths) {
        QFile::remove(path);
        QDir().rmdir(QFileInfo(path).absolutePath());
    }
}
