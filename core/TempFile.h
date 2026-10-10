/** @file TempFile.h
 * @brief 临时文件路径及其独占目录的生命周期管理。
 */
#pragma once
#include <filesystem>
#include <fstream>
#include <string>

namespace core {
/**
 * @brief 在单例独占的子目录内获取临时路径，退出时只清理自己拥有的目录。
 */
class TempFile {
public:
    ~TempFile() noexcept;
    // 单例持有唯一清理权；禁止复制或移动到另一个生命周期。
    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;

    TempFile(TempFile&&) = delete;
    TempFile& operator=(TempFile&&) = delete;

    static TempFile& instance();
    /**
     * @brief 获取尚未创建的临时文件路径，可从多个线程调用。
     */
    std::filesystem::path path() const;
    /**
     * @return 获取临时文件的fstream对象
     */
    std::fstream stream() const;

private:
    TempFile();
    std::filesystem::path path_;
    // 生成随机字符串
    static std::string random_string(size_t length);
};
}
