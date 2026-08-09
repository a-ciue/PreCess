#ifndef OBJ_MESH_IO_H
#define OBJ_MESH_IO_H
#include "ComponentData.h"
#include "MeshData.h"
#include <filesystem>
#include <iosfwd>
#include <optional>

class ObjMeshIO {
public:
	/**
	 * @brief 从文件加载 OBJ 模型，按 shape(group) 分组拆分为多个组件。
	 *
	 * 每个 shape 生成一个 ComponentData，其 MeshData 自包含：
	 * 只携带本组面实际引用的顶点（全局点索引重映射为组件内局部点索引）。
	 * @param filename 要加载的文件的路径。
	 * @return 加载的组件列表；文件解析失败时返回 std::nullopt。
	 */
	static std::optional<ComponentDatas> loadFromFile(const std::filesystem::path& filename);

	/**
	 * @brief 将单个 MeshData 保存到指定的输出流（如文件输出流）中。
	 *
	 * 零拷贝写出全部点与面（点索引 1-based），不写 object 分组行；
	 * 适合 CTMesh 转换等只有单网格、无需组件语义的场合。
	 * @param mesh 要保存的 MeshData 对象。
	 * @param os 用于写入数据的输出流。
	 */
	static void saveToFile(const MeshData& mesh, std::ostream& os);

	/**
	 * @brief 将组件列表保存到指定的输出流（如文件输出流）中。
	 *
	 * 每个组件写为一个 OBJ object（组件名作为 object 名），
	 * 与 loadFromFile 按 shape 拆回组件对应；点索引按 OBJ 约定
	 * 1-based 且全文件连续，跨组件自动累加偏移。
	 * @param components 要保存的组件列表（无网格的组件会被跳过）。
	 * @param os 用于写入数据的输出流。
	 */
	static void saveToFile(const ComponentDatas& components, std::ostream& os);
	/**
	 * @brief 同上，入参为只读指针视图。
	 *
	 * 供只持有 const ComponentData* 的调用方（如按 id 从 ModelLayer 查询）零拷贝复用。
	 */
	static void saveToFile(const std::vector<const ComponentData*>& components, std::ostream& os);
};

#endif // !OBJ_MESH_IO_H