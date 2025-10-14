#ifndef LIB_MESHB_IO_H
#define LIB_MESHB_IO_H
#include "MeshData.h"
#include <cstdint>
#include <filesystem>

/**
 * @brief 封装 LibMeshb 库做 .mesh/.meshb 文件读写
 */
class LibMeshbIO {
public:
    /**
     * @brief 读取文件，更新MeshData
     * @param mesh_data 待更新的核心网格数据结构
     * @return 成功返回 true，失败返回 false
     */
    static bool read(const std::filesystem::path& input_path, MeshData& mesh_data);

    /**
     * @brief 从指定的 MeshData 对象写出数据。
     * @param mesh_data 用于更新的 MeshData 对象。
     * @return 成功返回 true，失败返回 false
     */
    static bool write(int64_t meshb_idx, const MeshData& mesh_data);
};

#endif
