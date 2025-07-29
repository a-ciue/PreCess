// TempFile.cpp
#include "TempFile.h"
#include <random>

namespace core {
TempFile::TempFile() {
    auto temp_dir = std::filesystem::temp_directory_path();
    std::string filename = "tmp_" + random_string(8) + ".tmp";
    path_ = temp_dir / filename;
    std::ofstream ofs(path_);
    ofs.close();
}
TempFile::~TempFile() {
    if (std::filesystem::exists(path_)) {
        std::filesystem::remove(path_);
    }
}

const std::filesystem::path& TempFile::path() const
{
    return path_;
}

std::fstream TempFile::stream() {
    return std::fstream(path_, std::ios::in | std::ios::out);
}
std::string TempFile::random_string(size_t length)
{
    static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    static std::mt19937 rng { std::random_device {}() };
    static std::uniform_int_distribution<> dist(0, sizeof(charset) - 2);
    std::string str;
    for (size_t i = 0; i < length; ++i) {
        str += charset[dist(rng)];
    }
    return str;
}
} // namespace core
