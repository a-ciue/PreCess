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
	 * @brief 从 OBJ 文件加载网格，可选按 g 分组拆分组件。
	 *
	 * 每个组件仅包含其面引用的顶点。组名末尾的数字保存为组号，前缀作为属性名。
	 * 一个组件有该属性时，缺少该属性的面填 0。
	 * @param filename 要加载的文件的路径。
	 * @param split_groups 是否在各对象内继续按面分组拆分组件。
	 * @return 加载的组件列表；文件解析失败时返回 std::nullopt。
	 */
	static std::optional<ComponentDatas> loadFromFile(const std::filesystem::path& filename, bool split_groups = true);

	/**
	 * @brief 将单个 MeshData 保存到指定的输出流（如文件输出流）中。
	 *
	 * 零拷贝写出全部点与面（点索引 1-based），不写 object 分组行；
	 * 适合只有单网格、无需组件语义的场合。
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
