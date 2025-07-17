/**
 * @file SystemPluginManager.h
 * @author 张家僮(htxz_6a6@163.com)
 */
#ifndef SYSTEM_PLUGIN_MANAGER_H
#define SYSTEM_PLUGIN_MANAGER_H
#include "SystemRegisterBase.h"

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <filesystem>
#include <memory>

namespace systems {
class SystemPluginManager {
public:
	/**
	 * @brief 注册一个插件
     * @param plugin_path 待注册插件的路径，通常指.dll文件路径
	 * @return 注册是否成功
	 */
	bool registerPlugin(const std::filesystem::path& plugin_path);
	/**
	 * @brief 注销一个插件
	 * @param plugin_path 待注销的插件的路径，通常指.dll文件路径
	 */
	void unregisterPlugin(const std::filesystem::path& plugin_path);
	/**
	 * @brief 获取已注册的插件名称列表
	 */
	[[nodiscard]]std::vector<std::string> getPluginNames() const;

    bool addSystemRegister(const std::string& system_name, std::unique_ptr<SystemRegisterBase> system_register);
    void removeSystemRegister(const std::string& system_name);

private:
    std::unordered_map<std::string, std::unique_ptr<SystemRegisterBase>> system_registers_;
	// TODO: 基于插件dll名称识别是否读取过插件并不鲁棒，待对dll插件文件取md5hash作为插件标识
    std::unordered_set<std::string> plugin_names_;	//> 已注册的插件名称列表（不包括路径和拓展名）
};
}
#endif // SYSTEM_PLUGIN_MANAGER_H