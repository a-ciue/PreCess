//
// Created by 徐昊阳 on 4/8/25.
//

#ifndef TRIANGULATION_TESTMODEL_H
#define TRIANGULATION_TESTMODEL_H

#include "Model.h"
#include <memory>
#include <unordered_map>

// 用于测试的 dummy Patch、Block、Group 已在 Model.h 中定义
class TestModel : public Model {
public:
    /**
     * @brief 构造函数，使用一个 dummy CTMesh 代替真实数据
     */
    TestModel() : Model(createDummyMesh()) {}

    /**
     * @brief 直接设置 patches（仅用于测试，模拟真实数据）
     */
    void setPatches(const std::unordered_map<int, std::unique_ptr<Patch>> &patches) {
        patches_ = patches;
    }
    /**
     * @brief 直接设置 blocks（仅用于测试，模拟真实数据）
     */
    void setBlocks(const std::unordered_map<int, std::unique_ptr<Block>> &blocks) {
        blocks_ = blocks;
    }
    /**
     * @brief 直接设置 groups（仅用于测试，模拟真实数据）
     */
    void setGroups(const std::unordered_map<int, std::unique_ptr<Group>> &groups) {
        groups_ = groups;
    }

private:
    /**
     * @brief 创建一个 dummy CTMesh 对象用于 Model 构造，不关心内部数据。
     */
    static std::unique_ptr<MeshLib::CTMesh> createDummyMesh() {
        // 这里可以返回一个空的 dummy 对象，只要不为 nullptr即可
        // 实际项目中需要一个合法的 CTMesh，但测试中只为绕过构造检查
        return std::unique_ptr<MeshLib::CTMesh>(new MeshLib::CTMesh() );
    }
    //Model(std::unique_ptr<MeshLib::CTMesh> mesh);


    // 为了方便测试，这里直接覆盖（访问权限为 protected 或 public）Model 内部数据
    // 如果 Model 的内部数据是 private，则需要在 Model 中添加 friend class TestModel; 来允许访问

    // 假设我们在 Model 类中加了：
    //   friend class TestModel;
    // 这样就可以直接赋值给 patches_, blocks_ 和 groups_。
};

#endif //TRIANGULATION_TESTMODEL_H
