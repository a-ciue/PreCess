/**
 * @file TestTempFile.cpp
 * @brief 临时目录的进程隔离、并发路径和退出清理回归测试。
 */
#include "TempFile.h"

#include <QCoreApplication>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <set>
#include <thread>
#include <type_traits>
#include <vector>

static_assert(!std::is_copy_constructible_v<core::TempFile>);
static_assert(!std::is_move_constructible_v<core::TempFile>);
static_assert(!std::is_move_assignable_v<core::TempFile>);
static_assert(std::is_nothrow_destructible_v<core::TempFile>);

namespace {
void ensureApplication()
{
    static int argc = 1;
    static char name[] = "TestTempFile";
    static char* argv[] = { name, nullptr };
    static QCoreApplication application(argc, argv);
}

void startProcess(QProcess& process, const QString& sandbox, const QString& mode = {})
{
    ensureApplication();
    auto environment = QProcessEnvironment::systemEnvironment();
    // 子进程限定在测试独占沙箱内，旧实现的失败也不会清理真实用户临时文件。
    for (const auto& name : { "TMP", "TEMP", "TMPDIR" })
        environment.insert(name, sandbox);
    process.setProcessEnvironment(environment);
    process.start(QString::fromUtf8(TEMPFILE_PROCESS_PATH), QStringList { mode });
    REQUIRE(process.waitForStarted(5000));
}

QByteArray readLine(QProcess& process)
{
    while (!process.canReadLine()) {
        const bool ready = process.waitForReadyRead(5000);
        INFO(process.readAllStandardError().toStdString());
        REQUIRE(ready);
    }
    return process.readLine().trimmed();
}

void finishProcess(QProcess& process)
{
    REQUIRE(process.write("exit\n") > 0);
    REQUIRE(process.waitForFinished(5000));
    INFO(process.readAllStandardError().toStdString());
    REQUIRE(process.exitStatus() == QProcess::NormalExit);
    REQUIRE(process.exitCode() == 0);
}
}

TEST_CASE("TempFile isolates process cleanup and preserves live peers", "[core][TempFile]")
{
    QTemporaryDir sandbox;
    REQUIRE(sandbox.isValid());
    const auto shared_root = std::filesystem::path(sandbox.path().toStdU16String()) / "PreCess";
    std::filesystem::create_directory(shared_root);
    const auto foreign_file = shared_root / "foreign.txt";
    std::ofstream(foreign_file) << "unowned";

    QProcess first;
    QProcess second;
    startProcess(first, sandbox.path());
    const auto first_path = QString::fromUtf8(readLine(first));
    startProcess(second, sandbox.path());
    const auto second_path = QString::fromUtf8(readLine(second));
    const auto first_dir = std::filesystem::path(first_path.toStdU16String()).parent_path();
    const auto second_dir = std::filesystem::path(second_path.toStdU16String()).parent_path();
    CHECK(first_dir != second_dir);

    finishProcess(first);
    CHECK_FALSE(std::filesystem::exists(first_dir));
    REQUIRE(QFile::exists(second_path));
    CHECK(std::filesystem::exists(foreign_file));
    REQUIRE(second.write("verify\n") > 0);
    CHECK(readLine(second) == "ok");
    finishProcess(second);
    CHECK_FALSE(std::filesystem::exists(second_dir));
    CHECK(std::filesystem::exists(foreign_file));
}

TEST_CASE("TempFile generates unique paths across concurrent threads", "[core][TempFile]")
{
    auto& temporary_files = core::TempFile::instance();
    std::array<std::vector<std::filesystem::path>, 8> batches;
    {
        std::vector<std::jthread> threads;
        for (auto& batch : batches) {
            threads.emplace_back([&temporary_files, &batch] {
                for (int index = 0; index < 128; ++index)
                    batch.push_back(temporary_files.path());
            });
        }
    }
    std::set<std::filesystem::path> paths;
    for (const auto& batch : batches) {
        for (const auto& path : batch) {
            CHECK(std::filesystem::is_directory(path.parent_path()));
            CHECK(paths.insert(path).second);
        }
    }
}

#ifdef Q_OS_WIN
TEST_CASE("TempFile cleanup failure does not terminate the process", "[core][TempFile]")
{
    QTemporaryDir sandbox;
    REQUIRE(sandbox.isValid());
    QProcess process;
    startProcess(process, sandbox.path(), "locked");
    const auto path = QString::fromUtf8(readLine(process));
    finishProcess(process);
    // 占用期间无法删除的文件留在本次沙箱内，由沙箱在句柄释放后收尾。
    CHECK(QFile::exists(path));
}
#endif
