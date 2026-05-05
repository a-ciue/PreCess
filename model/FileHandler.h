/**
 * @file FileHandler.h
 * @brief 声明 FileHandler 类，用于处理所有与文件 IO 相关的操作
 *
 * FileHandler 是一个单例类，负责读取样条文件和网格文件，
 * 将文件数据转换为 ModelData 对象，并支持将 ModelData 数据写出到文件中。
 * 该类封装了文件操作和数据转换的具体实现，从而实现与模型管理的彻底解耦。
 *
 * @author 徐昊阳 haoyangxu06@gmail.com
 * @date 2025/3/22
 */
#ifndef FILEHANDLER_H
#define FILEHANDLER_H

#include "ModelData.h"
#include <memory>
#include <filesystem>

 /**
  * @brief FileHandler 单例类
  *
  * 负责所有与文件 IO 相关的操作，包括：
  * - 读取样条文件并转换为 ModelData 对象
  * - 读取网格文件并转换为 ModelData 对象
  * - 将 ModelData 数据写出到文件
  */
class FileHandler {
public:
    static FileHandler& instance() {
        static FileHandler instance;
        return instance;
    }

    /**
     * @brief 读取样条文件并生成 ModelData 对象
     * @param spline_path 样条文件的 URL 路径
     * @return 成功时返回 std::unique_ptr<ModelData>，失败返回 nullptr
     */
    std::unique_ptr<ModelData> readSpline(const std::filesystem::path& spline_path);

    bool writeSpline(GeometryData& spline, const std::filesystem::path& target_path);

private:
    // 禁止外部构造和拷贝
    FileHandler() = default;
    ~FileHandler() = default;
    FileHandler(const FileHandler&) = delete;
    FileHandler& operator=(const FileHandler&) = delete;
};

#endif // FILEHANDLER_H
