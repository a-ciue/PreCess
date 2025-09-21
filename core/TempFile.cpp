// TempFile.cpp
#include "TempFile.h"
#include <random>

namespace core {
TempFile::TempFile()
{
    auto temp_dir = std::filesystem::temp_directory_path();
    std::string folder = "PreCess";
    path_ = temp_dir / folder;
    std::filesystem::create_directory(path_);
}
TempFile::~TempFile()
{
    if (std::filesystem::exists(path_)) {
        std::filesystem::remove_all(path_);
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
    static std::mt19937 rng { std::random_device {}() };
    static std::uniform_int_distribution<> dist(0, sizeof(charset) - 2);
    std::string str;
    for (size_t i = 0; i < length; ++i) {
        str += charset[dist(rng)];
    }
    return str;
}
} // namespace core
