/** @file TestQModelDeletion.cpp
 * @brief Qt 删除入口的忙时拒绝与撤销状态通知回归测试
 */
#include "ComponentData.h"
#include "ModelLayer.h"
#include "QModelManager.h"
#include "UndoStack.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Qt deletion refuses occupied operations without emitting removal", "[QModelManager]")
{
    int argc = 1;
    char name[] = "TestQModelDeletion";
    char* argv[] = { name, nullptr };
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    QModelManager manager((directory.path() + "/isolated/Test.exe").toStdString());
    auto& model = *manager.getModelManager();
    // 初始数据不记入测试关注的删除历史。
    model.setUndoRecorder(nullptr);
    ComponentDatas components;
    components.push_back(std::make_unique<ComponentData>());
    const auto mid = model.addModel("fixture", std::move(components));
    const auto cid = model.modelById(mid)->componentIds()[0];
    int removed = 0;
    QObject::connect(&manager, &QModelManager::modelRemoved, [&] { ++removed; });
    auto operation = model.beginWriteOperation(false);
    CHECK_NOTHROW(manager.removeModel(mid));
    CHECK_NOTHROW(manager.removeComponent(cid));
    CHECK(removed == 0);
    CHECK(model.modelById(mid));
    CHECK(model.findComponent(cid));
    operation.reset();
    CHECK_NOTHROW(manager.removeComponent(cid));
    CHECK_FALSE(model.findComponent(cid));
    // 不经适配器发命令的修改也应通过模型通知转发删除信号。
    model.removeModel(mid);
    CHECK(removed == 1);
}

TEST_CASE("Qt undo adaptor publishes opening an empty preview", "[QUndoStackAdaptor]")
{
    ModelLayer model;
    UndoStack undo(model);
    QUndoStackAdaptor adaptor(undo);
    int changed = 0;
    QObject::connect(&adaptor, &QUndoStackAdaptor::stackChanged, [&] {
        ++changed;
        CHECK(adaptor.scopeActive());
        CHECK(adaptor.canUndo());
        CHECK_FALSE(adaptor.canRedo());
        CHECK(adaptor.undoLabel() == "preview");
    });
    REQUIRE(undo.beginScope("preview"));
    CHECK(changed == 1);
    undo.setOnChanged({ });
}
