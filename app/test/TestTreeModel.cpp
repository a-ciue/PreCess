/** @file TestTreeModel.cpp
 * @brief 对象树增量行信号与展开状态身份回归测试
 */
#include "ComponentData.h"
#include "ComponentOperator.h"
#include "GeometryData.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "ModelOperator.h"
#include "QModelQuery.h"
#include "SessionQuery.h"
#include "TreeModel.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QPersistentModelIndex>
#include <QTimer>
#include <catch2/catch_test_macros.hpp>
#include <memory>

namespace {
QCoreApplication& application()
{
    static int argc = 1;
    static char name[] = "TestTreeModel";
    static char* argv[] = { name, nullptr };
    static QCoreApplication app(argc, argv);
    return app;
}

void waitMs(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}
}

TEST_CASE("TreeModel applies structural changes through row signals", "[TreeModel]")
{
    application();
    ModelLayer model;
    session::SessionQuery query(model);
    QModelQuery model_query(&query);
    TreeModel tree;
    tree.setModelQuery(&model_query);

    int resets = 0;
    int inserted = 0;
    int removed = 0;
    QObject::connect(&tree, &QAbstractItemModel::modelReset, [&] { ++resets; });
    QObject::connect(&tree, &QAbstractItemModel::rowsInserted, [&] { ++inserted; });
    QObject::connect(&tree, &QAbstractItemModel::rowsRemoved, [&] { ++removed; });

    REQUIRE(tree.refresh());
    CHECK(resets == 1);
    CHECK(tree.rowCount() == 0);

    ComponentDatas components;
    auto mesh_component = std::make_unique<ComponentData>();
    mesh_component->mesh = std::make_unique<MeshData>();
    components.push_back(std::move(mesh_component));
    auto geometry_component = std::make_unique<ComponentData>();
    geometry_component->geometry = std::make_unique<GeometryData>();
    components.push_back(std::move(geometry_component));

    const Index model_id = model.addModel("incremental", std::move(components));
    REQUIRE(tree.syncModelAdded(model_id));
    CHECK(resets == 1);
    CHECK(inserted == 1);
    CHECK(removed == 0);
    REQUIRE(tree.rowCount() == 1);

    const QModelIndex model_index = tree.index(0, 0);
    REQUIRE(model_index.isValid());
    CHECK(tree.data(model_index, TreeModel::NameRole).toString() == "incremental");
    REQUIRE(tree.rowCount(model_index) == 2);

    const Index geometry_id = model.modelById(model_id)->componentIds()[1];
    const QModelIndex geometry_index = tree.findIndexByNodeId(geometry_id, 1);
    REQUIRE(geometry_index.isValid());
    CHECK(geometry_index.parent() == model_index);
    REQUIRE(tree.rowCount(geometry_index) == 1);
    CHECK(tree.data(tree.index(0, 0, geometry_index), TreeModel::NodeTypeRole).toInt()
        == static_cast<int>(TreeNode::NodeType::Geometry));

    const QPersistentModelIndex persistent_geometry(geometry_index);
    TreeNode* geometry_node = static_cast<TreeNode*>(geometry_index.internalPointer());

    const Index other_model_id = model.addModel("appended", { });
    REQUIRE(tree.syncModelAdded(other_model_id));
    CHECK(resets == 1);
    CHECK(inserted == 2);
    REQUIRE(tree.rowCount() == 2);
    CHECK(persistent_geometry.isValid());
    CHECK(persistent_geometry.internalPointer() == geometry_node);
    CHECK(persistent_geometry.parent().internalPointer() == model_index.internalPointer());

    auto model_operator = model.getModelOperator(model_id);
    REQUIRE(model_operator);
    auto created_component = std::make_unique<ComponentData>();
    created_component->geometry = std::make_unique<GeometryData>();
    const Index created_id = model_operator->addGeometryComponent(std::move(created_component));

    tree.queueComponentChanged(created_id);
    CHECK(resets == 1);
    REQUIRE(tree.rowCount(model_index) == 3);
    CHECK(tree.findIndexByNodeId(created_id, 1).isValid());

    model.removeComponent(created_id);
    REQUIRE(tree.syncComponentRemoved(created_id));
    CHECK(resets == 1);
    CHECK(removed == 1);
    REQUIRE(tree.rowCount(model_index) == 2);
    CHECK_FALSE(tree.findIndexByNodeId(created_id, 1).isValid());

    model.removeModel(model_id);
    REQUIRE(tree.syncModelRemoved(model_id));
    CHECK(resets == 1);
    CHECK(removed == 2);
    REQUIRE(tree.rowCount() == 1);
    CHECK(tree.index(0, 0).isValid());
    CHECK(tree.rowCount(tree.index(0, 0)) == 0);
}

TEST_CASE("TreeModel emits data changes for mutated component names", "[TreeModel]")
{
    application();
    ModelLayer model;
    session::SessionQuery query(model);
    QModelQuery model_query(&query);
    TreeModel tree;
    tree.setModelQuery(&model_query);

    int data_changed = 0;
    QObject::connect(&tree, &QAbstractItemModel::dataChanged, [&] { ++data_changed; });

    REQUIRE(tree.refresh());
    ComponentDatas components;
    components.push_back(std::make_unique<ComponentData>());
    const Index model_id = model.addModel("names", std::move(components));
    REQUIRE(tree.syncModelAdded(model_id));

    const Index component_id = model.modelById(model_id)->componentIds().front();
    const QModelIndex component_index = tree.findIndexByNodeId(component_id, 1);
    REQUIRE(component_index.isValid());

    auto component_operator = model.getComponentOperator(component_id);
    REQUIRE(component_operator);
    component_operator->setName("renamed");
    REQUIRE(tree.syncComponentChanged(component_id));
    CHECK(data_changed > 0);
    CHECK(tree.data(component_index, TreeModel::NameRole).toString() == "renamed");
}


TEST_CASE("TreeModel reconciles changed models without rebuilding the tree", "[TreeModel]")
{
    application();
    ModelLayer model;
    session::SessionQuery query(model);
    QModelQuery model_query(&query);
    TreeModel tree;
    tree.setModelQuery(&model_query);

    int resets = 0;
    int inserted = 0;
    int removed = 0;
    QObject::connect(&tree, &QAbstractItemModel::modelReset, [&] { ++resets; });
    QObject::connect(&tree, &QAbstractItemModel::rowsInserted, [&] { ++inserted; });
    QObject::connect(&tree, &QAbstractItemModel::rowsRemoved, [&] { ++removed; });

    REQUIRE(tree.refresh());
    CHECK(resets == 1);

    ComponentDatas components;
    components.push_back(std::make_unique<ComponentData>());
    const Index model_id = model.addModel("reconcile", std::move(components));
    REQUIRE(tree.syncModelAdded(model_id));
    const QModelIndex model_index = tree.index(0, 0);
    REQUIRE(tree.rowCount(model_index) == 1);

    auto model_operator = model.getModelOperator(model_id);
    REQUIRE(model_operator);
    auto appended = std::make_unique<ComponentData>();
    appended->geometry = std::make_unique<GeometryData>();
    const Index appended_id = model_operator->addGeometryComponent(std::move(appended));

    REQUIRE(tree.syncModelChanged(model_id));
    CHECK(resets == 1);
    CHECK(inserted == 2);
    REQUIRE(tree.rowCount(model_index) == 2);
    CHECK(tree.findIndexByNodeId(appended_id, 1).isValid());

    const Index removed_id = model.modelById(model_id)->componentIds().front();
    model.removeComponent(removed_id);
    REQUIRE(tree.syncModelChanged(model_id));
    CHECK(resets == 1);
    CHECK(removed == 1);
    REQUIRE(tree.rowCount(model_index) == 1);
    CHECK_FALSE(tree.findIndexByNodeId(removed_id, 1).isValid());
    CHECK(tree.findIndexByNodeId(appended_id, 1).isValid());
}

TEST_CASE("TreeModel removes mesh and geometry nodes incrementally", "[TreeModel]")
{
    application();
    ModelLayer model;
    session::SessionQuery query(model);
    QModelQuery model_query(&query);
    TreeModel tree;
    tree.setModelQuery(&model_query);

    int resets = 0;
    int removed = 0;
    QObject::connect(&tree, &QAbstractItemModel::modelReset, [&] { ++resets; });
    QObject::connect(&tree, &QAbstractItemModel::rowsRemoved, [&] { ++removed; });

    REQUIRE(tree.refresh());
    ComponentDatas components;
    auto component = std::make_unique<ComponentData>();
    component->mesh = std::make_unique<MeshData>();
    component->geometry = std::make_unique<GeometryData>();
    components.push_back(std::move(component));
    const Index model_id = model.addModel("subnodes", std::move(components));
    REQUIRE(tree.syncModelAdded(model_id));

    const Index component_id = model.modelById(model_id)->componentIds().front();
    const QModelIndex component_index = tree.findIndexByNodeId(component_id, 1);
    REQUIRE(component_index.isValid());
    REQUIRE(tree.rowCount(component_index) == 2);

    auto component_operator = model.getComponentOperator(component_id);
    REQUIRE(component_operator);
    component_operator->removeMesh();
    model.flushNotifications();
    REQUIRE(tree.syncComponentChanged(component_id));
    CHECK(resets == 1);
    CHECK(removed == 1);
    REQUIRE(tree.rowCount(component_index) == 1);
    CHECK(tree.data(tree.index(0, 0, component_index), TreeModel::NodeTypeRole).toInt()
        == static_cast<int>(TreeNode::NodeType::Geometry));

    component_operator->removeGeometry();
    model.flushNotifications();
    REQUIRE(tree.syncComponentChanged(component_id));
    CHECK(resets == 1);
    CHECK(removed == 2);
    CHECK(tree.rowCount(component_index) == 0);
}

TEST_CASE("TreeModel coalesces data changes and keeps structural inserts immediate", "[TreeModel]")
{
    application();
    ModelLayer model;
    session::SessionQuery query(model);
    QModelQuery model_query(&query);
    TreeModel tree;
    tree.setModelQuery(&model_query);

    REQUIRE(tree.refresh());
    ComponentDatas components;
    components.push_back(std::make_unique<ComponentData>());
    const Index model_id = model.addModel("coalesce", std::move(components));
    REQUIRE(tree.syncModelAdded(model_id));

    const Index component_id = model.modelById(model_id)->componentIds().front();
    const QModelIndex component_index = tree.findIndexByNodeId(component_id, 1);
    REQUIRE(component_index.isValid());

    int data_changed = 0;
    QObject::connect(&tree, &QAbstractItemModel::dataChanged, [&] { ++data_changed; });

    auto component_operator = model.getComponentOperator(component_id);
    REQUIRE(component_operator);
    component_operator->setName("renamed");
    tree.queueComponentChanged(component_id);
    tree.queueComponentChanged(component_id);
    CHECK(data_changed == 0);

    waitMs(120);
    CHECK(data_changed > 0);
    CHECK(tree.data(component_index, TreeModel::NameRole).toString() == "renamed");

    auto model_operator = model.getModelOperator(model_id);
    REQUIRE(model_operator);
    auto created = std::make_unique<ComponentData>();
    created->geometry = std::make_unique<GeometryData>();
    const Index created_id = model_operator->addGeometryComponent(std::move(created));
    CHECK_FALSE(tree.findIndexByNodeId(created_id, 1).isValid());
    tree.queueComponentChanged(created_id);
    CHECK(tree.findIndexByNodeId(created_id, 1).isValid());
}
