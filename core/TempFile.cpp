/** @file TempFile.cpp
 * @brief 临时目录的独占创建和进程退出清理。
 */
#include "TempFile.h"
#include <random>
#include <system_error>
#include <utility>

namespace core {
TempFile::TempFile()
{
    const auto root = std::filesystem::temp_directory_path() / "PreCess";
    std::filesystem::create_directories(root);
    // 先原子创建独占子目录；文件名随机不足以保护共享目录的清理边界。
    for (int attempt = 0; attempt < 64; ++attempt) {
        auto candidate = root / ("run_" + random_string(20));
        std::error_code error;
        if (std::filesystem::create_directory(candidate, error)) {
            path_ = std::move(candidate);
            return;
        }
        if (error && error != std::errc::file_exists)
            throw std::filesystem::filesystem_error("Cannot create temporary directory", candidate, error);
    }
    throw std::filesystem::filesystem_error("Cannot reserve unique temporary directory", root, std::make_error_code(std::errc::file_exists));
}
TempFile::~TempFile() noexcept
{
    try {
        std::filesystem::remove_all(path_);
    } catch (...) {
        // 文件占用等退出清理失败只能留下本实例的残留，不能让析构终止进程。
    }
}

TempFile& TempFile::instance()
{
    static TempFile instance;
    return instance;
}

std::filesystem::path TempFile::path() const
{
    return path_ / ("temp_" + random_string(20) + ".tmp");
}

std::fstream TempFile::stream() const
{
    return std::fstream(path(), std::ios::in | std::ios::out);
}
std::string TempFile::random_string(size_t length)
{
    constexpr char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    thread_local std::mt19937 rng { std::random_device {}() };
    thread_local std::uniform_int_distribution<> dist(0, sizeof(charset) - 2);
    std::string str;
    for (size_t i = 0; i < length; ++i) {
        str += charset[dist(rng)];
    }
    return str;
}
} // namespace core
