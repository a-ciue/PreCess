/**
 * @file FileHandler.h
 * @brief 声明 FileHandler 类，用于处理所有与文件 IO 相关的操作
 *
 * FileHandler 是一个单例类，负责读取样条文件和网格文件，
 * 将文件数据转换为 Model 对象，并支持将 Model 数据写出到文件中。
 * 该类封装了文件操作和数据转换的具体实现，从而实现与模型管理的彻底解耦。
 *
 * @author 徐昊阳 haoyangxu06@gmail.com
 * @date 2025/3/22
 */
#ifndef FILEHANDLER_H
#define FILEHANDLER_H

#include <QString>
#include <QUrl>
#include <memory>
#include "Model.h"
#include "ToolMesh.h"

/**
 * @brief FileHandler 单例类
 *
 * 负责所有与文件 IO 相关的操作，包括：
 * - 读取样条文件并转换为 Model 对象
 * - 读取网格文件并转换为 Model 对象
 * - 将 Model 数据写出到文件
 */
class FileHandler {
public:
    static FileHandler& instance() {
        static FileHandler instance;
        return instance;
    }

    /**
     * @brief 读取样条文件并生成 Model 对象
     * @param spline_path 样条文件的 URL 路径
     * @return 成功时返回 std::unique_ptr<Model>，失败返回 nullptr
     */
    std::unique_ptr<Model> readSpline(const QUrl& spline_path);

    /**
     * @brief 读取网格文件并生成 Model 对象
     * @param mesh_path 网格文件的 URL 路径
     * @return 成功时返回 std::unique_ptr<Model>，失败返回 nullptr
     */
    std::unique_ptr<Model> readMesh(const QUrl& mesh_path);

    /**
     * @brief 将 Model 数据写入文件
     * @param model 指向 Model 对象的指针
     * @param targetPath 输出文件的路径
     * @param renderMode 渲染模式（"Face", "Block", "Group"）
     * @param extension 输出文件的扩展名
     * @return 成功返回 true，失败返回 false
     */
    bool writeMesh(Model* model, const QString& targetPath, const QString& renderMode, const QString& extension);

private:
    // 禁止外部构造和拷贝
    FileHandler() = default;
    ~FileHandler() = default;
    FileHandler(const FileHandler&) = delete;
    FileHandler& operator=(const FileHandler&) = delete;
};

#endif // FILEHANDLER_H
