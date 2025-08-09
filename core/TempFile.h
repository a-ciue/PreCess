// TempFile.h
// 临时文件的RAII实现
#pragma once
#include <filesystem>
#include <fstream>
#include <string>

namespace core {
/**
 * @brief 获取临时文件的RAII实现
 */
class TempFile {
public:
    TempFile();
    ~TempFile();
    // 对象拥有文件所有权（类unique_ptr，禁止拷贝构造和赋值
    TempFile(TempFile&) = delete;
    TempFile& operator=(TempFile&) = delete;

    TempFile(TempFile&&) = default;
    TempFile& operator=(TempFile&&) = default;
    /**
     * @brief （废弃）获取临时文件路径，推荐使用stream()代替path()
     */
    [[deprecated("Use stream() instead")]]
	const std::filesystem::path& path() const;
    /**
     * @return 获取临时文件的fstream对象
     */
    std::fstream stream();

private:
    std::filesystem::path path_;
    // 生成随机字符串
    static std::string random_string(size_t length);
};
}
