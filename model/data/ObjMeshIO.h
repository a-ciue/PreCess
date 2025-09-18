#ifndef OBJ_MESH_IO_H
#define OBJ_MESH_IO_H
#include "MeshData.h"
#include <filesystem>
#include <iosfwd>

class ObjMeshIO {
public:
	/**
	 * @brief 从文件加载 MeshData 对象。
	 * @param filename 要加载的文件的路径。
	 * @return 加载的 MeshData。如果加载失败，则返回空指针。
	 */
	static std::unique_ptr<MeshData> loadFromFile(const std::filesystem::path& filename);
	/**
	 * @brief 输出MeshData。
	 
	 将 MeshData 保存到指定的输出流（如文件输出流）中。
	 * @param mesh 要保存的 MeshData 对象。
	 * @param os 用于写入数据的输出流。
	 */
	static void saveToFile(const MeshData& mesh, std::ostream& os);
};

#endif // !OBJ_MESH_IO_H