/** @file TestTreeModel.cpp
 * @brief 对象树增量行信号与展开状态身份回归测试
 */
#include "ComponentData.h"
#include "ComponentOperator.h"
#include "GeometryBuilder.h"
#include "GeometryData.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "ModelOperator.h"
#include "QModelQuery.h"
#include "SessionQuery.h"
#include "TreeModel.h"
#include "TreeModelTestHelpers.h"

#include <QCoreApplication>
#include <QPersistentModelIndex>
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

std::unique_ptr<MeshData> makeCountMesh(int face_count, int solid_count)
{
    auto mesh = std::make_unique<MeshData>();
    mesh->vertex_positions_ = { { 0.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0 } };
    mesh->vertex_count_ = 3;
    if (face_count > 0)
        mesh->face_vertices_offset_.assign(static_cast<size_t>(face_count + 1), 0);
    if (solid_count > 0)
        mesh->solid_types_.assign(static_cast<size_t>(solid_count), 0);
    return mesh;
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
    CHECK(model_query.getComponentSummary(-1).isEmpty());

    ComponentDatas components;
    auto mesh_component = std::make_unique<ComponentData>();
    mesh_component->mesh = std::make_unique<MeshData>();
    components.push_back(std::move(mesh_component));
    auto geometry_component = std::make_unique<ComponentData>();
    geometry_component->geometry = std::make_unique<GeometryData>();
    components.push_back(std::move(geometry_component));

    const Index model_id = model.addModel("incremental", std::move(components));
    tree.syncModelAdded(model_id);
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

    ComponentDatas other_components;
    auto other_component = std::make_unique<ComponentData>();
    other_component->mesh = std::make_unique<MeshData>();
    other_components.push_back(std::move(other_component));
    const Index other_model_id = model.addModel("appended", std::move(other_components));
    tree.syncModelAdded(other_model_id);
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
    tree.syncComponentRemoved(created_id);
    CHECK(resets == 1);
    CHECK(removed == 1);
    REQUIRE(tree.rowCount(model_index) == 2);
    CHECK_FALSE(tree.findIndexByNodeId(created_id, 1).isValid());

    model.removeModel(model_id);
    tree.syncModelRemoved(model_id);
    CHECK(resets == 1);
    CHECK(removed == 2);
    REQUIRE(tree.rowCount() == 1);
    REQUIRE(tree.index(0, 0).isValid());
    CHECK(tree.data(tree.index(0, 0), TreeModel::NameRole).toString() == "appended");
    CHECK(tree.rowCount(tree.index(0, 0)) == 1);
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
    auto component = std::make_unique<ComponentData>();
    component->mesh = std::make_unique<MeshData>();
    components.push_back(std::move(component));
    const Index model_id = model.addModel("names", std::move(components));
    tree.syncModelAdded(model_id);

    const Index component_id = model.modelById(model_id)->componentIds().front();
    const QModelIndex component_index = tree.findIndexByNodeId(component_id, 1);
    REQUIRE(component_index.isValid());

    auto component_operator = model.getComponentOperator(component_id);
    REQUIRE(component_operator);
    component_operator->setName("renamed");
    tree.syncComponentChanged(component_id);
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
    auto component = std::make_unique<ComponentData>();
    component->mesh = std::make_unique<MeshData>();
    components.push_back(std::move(component));
    const Index model_id = model.addModel("reconcile", std::move(components));
    tree.syncModelAdded(model_id);
    const QModelIndex model_index = tree.index(0, 0);
    REQUIRE(tree.rowCount(model_index) == 1);

    auto model_operator = model.getModelOperator(model_id);
    REQUIRE(model_operator);
    auto appended = std::make_unique<ComponentData>();
    appended->geometry = std::make_unique<GeometryData>();
    const Index appended_id = model_operator->addGeometryComponent(std::move(appended));

    tree.syncModelChanged(model_id);
    CHECK(resets == 1);
    CHECK(inserted == 2);
    REQUIRE(tree.rowCount(model_index) == 2);
    CHECK(tree.findIndexByNodeId(appended_id, 1).isValid());

    const Index removed_id = model.modelById(model_id)->componentIds().front();
    model.removeComponent(removed_id);
    tree.syncModelChanged(model_id);
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
    tree.syncModelAdded(model_id);

    const Index component_id = model.modelById(model_id)->componentIds().front();
    const QModelIndex component_index = tree.findIndexByNodeId(component_id, 1);
    REQUIRE(component_index.isValid());
    REQUIRE(tree.rowCount(component_index) == 2);

    auto component_operator = model.getComponentOperator(component_id);
    REQUIRE(component_operator);
    component_operator->removeMesh();
    model.flushNotifications();
    tree.syncComponentChanged(component_id);
    CHECK(resets == 1);
    CHECK(removed == 1);
    REQUIRE(tree.rowCount(component_index) == 1);
    CHECK(tree.data(tree.index(0, 0, component_index), TreeModel::NodeTypeRole).toInt()
        == static_cast<int>(TreeNode::NodeType::Geometry));

    component_operator->removeGeometry();
    model.flushNotifications();
    tree.syncComponentChanged(component_id);
    CHECK(resets == 1);
    CHECK(removed == 2);
    CHECK(tree.rowCount(component_index) == 0);
    CHECK(tree.findIndexByNodeId(component_id, 1).isValid());
    CHECK(tree.findIndexByNodeId(model_id, 0).isValid());
}

TEST_CASE("TreeModel shows a newly added subtype as visible", "[TreeModel]")
{
    application();
    ModelLayer model;
    session::SessionQuery query(model);
    QModelQuery model_query(&query);
    TreeModel tree;
    tree.setModelQuery(&model_query);

    REQUIRE(tree.refresh());
    ComponentDatas components;
    auto component = std::make_unique<ComponentData>();
    component->geometry = std::make_unique<GeometryData>();
    components.push_back(std::move(component));
    const Index model_id = model.addModel("new subtype", std::move(components));
    const Index component_id = model.modelById(model_id)->componentIds().front();
    tree.syncModelAdded(model_id);

    QModelIndex component_index = tree.findIndexByNodeId(component_id, 1);
    REQUIRE(component_index.isValid());
    REQUIRE(tree.rowCount(component_index) == 1);

    auto component_operator = model.getComponentOperator(component_id);
    REQUIRE(component_operator);
    component_operator->replaceMesh(std::make_unique<MeshData>());
    model.flushNotifications();
    tree.syncComponentChanged(component_id);

    component_index = tree.findIndexByNodeId(component_id, 1);
    REQUIRE(component_index.isValid());
    REQUIRE(tree.rowCount(component_index) == 2);
    QModelIndex mesh_index;
    for (int i = 0; i < tree.rowCount(component_index); ++i) {
        const QModelIndex child = tree.index(i, 0, component_index);
        if (tree.data(child, TreeModel::NodeTypeRole).toInt()
            == static_cast<int>(TreeNode::NodeType::Mesh)) {
            mesh_index = child;
            break;
        }
    }
    REQUIRE(mesh_index.isValid());
    CHECK(tree.data(mesh_index, TreeModel::IsVisibleRole).toBool());
    CHECK(tree.data(component_index, TreeModel::IsVisibleRole).toBool());
}

TEST_CASE("TreeModel keeps count child order and numbers", "[TreeModel]")
{
    application();
    ModelLayer model;
    session::SessionQuery query(model);
    QModelQuery model_query(&query);
    TreeModel tree;
    tree.setModelQuery(&model_query);

    REQUIRE(tree.refresh());
    ComponentDatas components;
    auto mesh_component = std::make_unique<ComponentData>();
    mesh_component->mesh = makeCountMesh(1, 1);
    components.push_back(std::move(mesh_component));
    auto geometry_component = std::make_unique<ComponentData>();
    geometry_component->geometry = std::make_unique<GeometryData>();
    geometry_component->geometry->setRootShape(
        GeometryBuilder::makeBox(0.0, 0.0, 0.0, 1.0, 2.0, 3.0));
    components.push_back(std::move(geometry_component));

    const Index model_id = model.addModel("count children", std::move(components));
    const auto component_ids = model.modelById(model_id)->componentIds();
    const Index mesh_component_id = component_ids[0];
    const Index geometry_component_id = component_ids[1];
    tree.syncModelAdded(model_id);

    auto child_of_type = [&tree](const QModelIndex& parent, TreeNode::NodeType type) {
        for (int i = 0; i < tree.rowCount(parent); ++i) {
            const QModelIndex child = tree.index(i, 0, parent);
            if (tree.data(child, TreeModel::NodeTypeRole).toInt() == static_cast<int>(type))
                return child;
        }
        return QModelIndex();
    };

    const QModelIndex mesh_component_index = tree.findIndexByNodeId(mesh_component_id, 1);
    const QModelIndex geometry_component_index = tree.findIndexByNodeId(geometry_component_id, 1);
    REQUIRE(mesh_component_index.isValid());
    REQUIRE(geometry_component_index.isValid());

    QModelIndex mesh_index = child_of_type(mesh_component_index, TreeNode::NodeType::Mesh);
    REQUIRE(mesh_index.isValid());
    CHECK(tree.data(mesh_index, TreeModel::NumberRole).toString() == "2");
    REQUIRE(tree.rowCount(mesh_index) == 2);
    CHECK(tree.data(tree.index(0, 0, mesh_index), TreeModel::NodeTypeRole).toInt()
        == static_cast<int>(TreeNode::NodeType::D2));
    CHECK(tree.data(tree.index(0, 0, mesh_index), TreeModel::NumberRole).toString() == "1");
    CHECK(tree.data(tree.index(1, 0, mesh_index), TreeModel::NodeTypeRole).toInt()
        == static_cast<int>(TreeNode::NodeType::D3));
    CHECK(tree.data(tree.index(1, 0, mesh_index), TreeModel::NumberRole).toString() == "1");

    QModelIndex geometry_index = child_of_type(
        geometry_component_index, TreeNode::NodeType::Geometry);
    REQUIRE(geometry_index.isValid());
    CHECK(tree.data(geometry_index, TreeModel::NumberRole).toString() == "27");
    REQUIRE(tree.rowCount(geometry_index) == 4);
    const TreeNode::NodeType geometry_types[] = {
        TreeNode::NodeType::Vertex,
        TreeNode::NodeType::Edge,
        TreeNode::NodeType::Face,
        TreeNode::NodeType::Solid,
    };
    const char* geometry_numbers[] = { "8", "12", "6", "1" };
    for (int i = 0; i < 4; ++i) {
        const QModelIndex child = tree.index(i, 0, geometry_index);
        CHECK(tree.data(child, TreeModel::NodeTypeRole).toInt()
            == static_cast<int>(geometry_types[i]));
        CHECK(tree.data(child, TreeModel::NumberRole).toString() == geometry_numbers[i]);
    }

    int resets = 0;
    QObject::connect(&tree, &QAbstractItemModel::modelReset, [&] { ++resets; });
    auto component_operator = model.getComponentOperator(mesh_component_id);
    REQUIRE(component_operator);
    component_operator->replaceMesh(makeCountMesh(1, 0));
    model.flushNotifications();
    tree.syncComponentChanged(mesh_component_id);

    mesh_index = child_of_type(mesh_component_index, TreeNode::NodeType::Mesh);
    REQUIRE(mesh_index.isValid());
    CHECK(tree.data(mesh_index, TreeModel::NumberRole).toString() == "1");
    REQUIRE(tree.rowCount(mesh_index) == 1);
    CHECK(tree.data(tree.index(0, 0, mesh_index), TreeModel::NodeTypeRole).toInt()
        == static_cast<int>(TreeNode::NodeType::D2));
    CHECK(tree.data(tree.index(0, 0, mesh_index), TreeModel::NumberRole).toString() == "1");

    component_operator->replaceMesh(makeCountMesh(0, 0));
    model.flushNotifications();
    tree.syncComponentChanged(mesh_component_id);

    mesh_index = child_of_type(mesh_component_index, TreeNode::NodeType::Mesh);
    REQUIRE(mesh_index.isValid());
    CHECK(tree.data(mesh_index, TreeModel::NumberRole).toString() == "0");
    CHECK(tree.rowCount(mesh_index) == 0);
    CHECK(resets == 0);
}

TEST_CASE("TreeModel resets state when the model query changes", "[TreeModel]")
{
    application();
    ModelLayer model;
    session::SessionQuery query(model);
    QModelQuery model_query(&query);
    TreeModel tree;
    tree.setModelQuery(&model_query);

    REQUIRE(tree.refresh());
    ComponentDatas components;
    auto component = std::make_unique<ComponentData>();
    component->mesh = std::make_unique<MeshData>();
    components.push_back(std::move(component));
    const Index model_id = model.addModel("query lifecycle", std::move(components));
    tree.syncModelAdded(model_id);
    REQUIRE(tree.rowCount() == 1);

    ModelLayer other_model;
    session::SessionQuery other_query(other_model);
    QModelQuery other_model_query(&other_query);
    ComponentDatas other_components;
    auto other_component = std::make_unique<ComponentData>();
    other_component->mesh = std::make_unique<MeshData>();
    other_components.push_back(std::move(other_component));
    other_model.addModel("replacement query", std::move(other_components));

    tree.setModelQuery(&other_model_query);
    REQUIRE(tree.rowCount() == 1);
    CHECK(tree.data(tree.index(0, 0), TreeModel::NameRole).toString() == "replacement query");

    REQUIRE_NOTHROW(tree.setModelQuery(nullptr));
    CHECK(tree.rowCount() == 0);
    REQUIRE_NOTHROW(tree.syncModelAdded(model_id));

    tree.setModelQuery(&model_query);
    REQUIRE(tree.refresh());
    REQUIRE(tree.rowCount() == 1);
    CHECK(tree.data(tree.index(0, 0), TreeModel::NameRole).toString() == "query lifecycle");
}

TEST_CASE("TreeModel resets fake node ids on full rebuild", "[TreeModel]")
{
    application();
    ModelLayer model;
    session::SessionQuery query(model);
    QModelQuery model_query(&query);
    TreeModel tree;
    tree.setModelQuery(&model_query);

    REQUIRE(tree.refresh());
    ComponentDatas components;
    auto component = std::make_unique<ComponentData>();
    component->mesh = std::make_unique<MeshData>();
    components.push_back(std::move(component));
    const Index model_id = model.addModel("fake ids", std::move(components));
    const Index component_id = model.modelById(model_id)->componentIds().front();
    tree.syncModelAdded(model_id);

    auto mesh_node_id = [&]() {
        const QModelIndex component_index = tree.findIndexByNodeId(component_id, 1);
        REQUIRE(component_index.isValid());
        REQUIRE(tree.rowCount(component_index) == 1);
        const QModelIndex mesh_index = tree.index(0, 0, component_index);
        REQUIRE(tree.data(mesh_index, TreeModel::NodeTypeRole).toInt()
            == static_cast<int>(TreeNode::NodeType::Mesh));
        return tree.data(mesh_index, TreeModel::NodeIdRole).toInt();
    };

    const int first_fake_id = mesh_node_id();
    REQUIRE(first_fake_id < 0);

    tree.setModelQuery(nullptr);
    tree.setModelQuery(&model_query);
    REQUIRE(tree.refresh());
    CHECK(mesh_node_id() == first_fake_id);
}

TEST_CASE("TreeModel consumes changes queued during a flush on the next timer", "[TreeModel]")
{
    application();
    ModelLayer model;
    session::SessionQuery query(model);
    QModelQuery model_query(&query);
    TreeModel tree;
    tree.setModelQuery(&model_query);

    REQUIRE(tree.refresh());
    ComponentDatas components;
    for (int i = 0; i < 2; ++i) {
        auto component = std::make_unique<ComponentData>();
        component->mesh = std::make_unique<MeshData>();
        components.push_back(std::move(component));
    }
    const Index model_id = model.addModel("flush reentrancy", std::move(components));
    const auto component_ids = model.modelById(model_id)->componentIds();
    const Index first_id = component_ids[0];
    const Index second_id = component_ids[1];
    tree.syncModelAdded(model_id);

    auto first_operator = model.getComponentOperator(first_id);
    auto second_operator = model.getComponentOperator(second_id);
    REQUIRE(first_operator);
    REQUIRE(second_operator);
    first_operator->setName("first-updated");
    second_operator->setName("second-updated");
    model.flushNotifications();

    int sync_finished = 0;
    bool queued_second = false;
    QObject::connect(&tree, &TreeModel::dataSyncFinished, [&] { ++sync_finished; });
    QObject::connect(&tree, &QAbstractItemModel::dataChanged, [&] {
        if (!queued_second) {
            queued_second = true;
            tree.queueComponentChanged(second_id);
        }
    });

    tree.queueComponentChanged(first_id);
    REQUIRE(waitForDataSync(tree));
    CHECK(sync_finished == 1);
    REQUIRE(waitForDataSync(tree));
    CHECK(sync_finished == 2);
    CHECK(tree.data(tree.findIndexByNodeId(first_id, 1), TreeModel::NameRole).toString()
        == "first-updated");
    CHECK(tree.data(tree.findIndexByNodeId(second_id, 1), TreeModel::NameRole).toString()
        == "second-updated");
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
    for (int i = 0; i < 2; ++i) {
        auto component = std::make_unique<ComponentData>();
        component->mesh = std::make_unique<MeshData>();
        components.push_back(std::move(component));
    }
    const Index model_id = model.addModel("coalesce", std::move(components));
    tree.syncModelAdded(model_id);

    const auto component_ids = model.modelById(model_id)->componentIds();
    const Index component_id = component_ids[0];
    const Index second_component_id = component_ids[1];
    const QModelIndex component_index = tree.findIndexByNodeId(component_id, 1);
    const QModelIndex second_component_index = tree.findIndexByNodeId(second_component_id, 1);
    REQUIRE(component_index.isValid());
    REQUIRE(second_component_index.isValid());

    int data_changed = 0;
    int inserted = 0;
    int removed = 0;
    int resets = 0;
    QObject::connect(&tree, &QAbstractItemModel::dataChanged, [&] { ++data_changed; });
    QObject::connect(&tree, &QAbstractItemModel::rowsInserted, [&] { ++inserted; });
    QObject::connect(&tree, &QAbstractItemModel::rowsRemoved, [&] { ++removed; });
    QObject::connect(&tree, &QAbstractItemModel::modelReset, [&] { ++resets; });

    auto component_operator = model.getComponentOperator(component_id);
    auto second_component_operator = model.getComponentOperator(second_component_id);
    REQUIRE(component_operator);
    REQUIRE(second_component_operator);
    component_operator->setName("renamed");
    second_component_operator->setName("renamed-second");
    tree.queueComponentChanged(component_id);
    tree.queueComponentChanged(component_id);
    tree.queueComponentChanged(second_component_id);
    tree.queueModelChanged(model_id);
    CHECK(data_changed == 0);

    REQUIRE(waitForDataSync(tree));
    CHECK(data_changed == 2);
    CHECK(inserted == 0);
    CHECK(removed == 0);
    CHECK(resets == 0);
    CHECK(tree.data(component_index, TreeModel::NameRole).toString() == "renamed");
    CHECK(tree.data(second_component_index, TreeModel::NameRole).toString() == "renamed-second");

    auto model_operator = model.getModelOperator(model_id);
    REQUIRE(model_operator);
    auto created = std::make_unique<ComponentData>();
    created->geometry = std::make_unique<GeometryData>();
    const Index created_id = model_operator->addGeometryComponent(std::move(created));
    CHECK_FALSE(tree.findIndexByNodeId(created_id, 1).isValid());
    tree.queueComponentChanged(created_id);
    CHECK(tree.findIndexByNodeId(created_id, 1).isValid());
}
