/**
 * @file OffMeshIO.h
 * @brief OFF(Geomview Object File Format)文件的读写封装
 *
 * @sa https://en.wikipedia.org/wiki/OFF_(file_format)
 * @sa https://people.sc.fsu.edu/~jburkardt/data/off/off.html
 */
#ifndef OFF_MESH_IO_H
#define OFF_MESH_IO_H

#include "MeshData.h"
#include <filesystem>

/**
 * @brief OFF 文件读写，数据只经 MeshData 的点与面承载
 *
 * 支持范围：
 * - ASCII：OFF / 4OFF / nOFF / 4nOFF 及带附加属性的变体（NOFF 法向、COFF 颜色、
 *   STOFF 纹理坐标等），文件头前允许出现 '#' 注释行；
 * - 二进制：头部为 "OFF BINARY" 的 Geomview 二进制格式，约定为小端字节序、
 *   int32 计数与索引、float32 坐标。
 *
 * 附加属性一律丢弃：软件当前不存储点/面的属性数据（法向、颜色、纹理等），
 * 它们位于顶点/面记录的行尾，读取时被忽略；4OFF 的齐次分量 w 同样忽略，
 * 坐标按原值取用。
 *
 * 每条顶点/面记录占一行，这是绝大多数 OFF 文件的实际写法；OFF 不携带边与
 * 体单元，故只填充 MeshData::vertex_positions_ / face_vertices_ /
 * face_vertices_offset_，edge_vertices_ 与 solid_* 保持为空。
 */
class OffMeshIO {
public:
    /**
     * @brief 读取 OFF 文件，结果覆盖写入 mesh
     * @param path 待读取文件路径，由 std::filesystem 处理本地编码
     * @param mesh 输出网格，成功时含点坐标与面连通性
     * @return 成功返回 true；文件打不开、头部非法、记录截断或面索引越界时返回 false
     */
    static bool read(const std::filesystem::path& path, MeshData& mesh);

    /**
     * @brief 把 mesh 的点与面写出为 ASCII OFF 文件
     * @param path 输出文件路径
     * @param mesh 待写出的网格，取 vertex_positions_ 与 face_vertices_/face_vertices_offset_；
     *        顶点数小于 3 的退化面与索引越界的脏面被跳过
     * @return 成功返回 true；打开失败或无有效面时返回 false
     */
    static bool write(const std::filesystem::path& path, const MeshData& mesh);
};

#endif // OFF_MESH_IO_H
