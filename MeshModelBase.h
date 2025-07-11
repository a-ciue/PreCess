#pragma once
/**
* @file MeshModelBase.h
* @author 张家僮(htxz_6a6@163.com)
*/

class MeshData;

/**
 * @brief 辅助网格数据结构，用于更新核心网格数据结构
 */
class MeshModelBase {
public:
    virtual ~MeshModelBase() = default;
	/**
     * @brief 更新核心网格数据结构
     * @param data 核心数据结构
	 */
	virtual void update(MeshData& data) = 0;
    //TODO: virtual void updateFrom(MeshData& data) = 0;
};