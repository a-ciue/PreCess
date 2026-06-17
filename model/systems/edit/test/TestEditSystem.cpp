#include "ComponentData.h"
#include "EditSystem.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "TrivialEditHandler.h"

#include <catch2/catch_test_macros.hpp>

using namespace systems::edit;

TEST_CASE("EditSystem::register&unregister")
{
    auto mesh_data = std::make_unique<MeshData>();
    mesh_data->init();

    auto c = std::make_unique<ComponentData>();
    c->id = -1; c->name = "Comp_0";
    c->mesh = std::move(mesh_data);
    ComponentDatas comps;
    comps.push_back(std::move(c));

    ModelLayer model_manager;
    REQUIRE_NOTHROW(model_manager.addModel("test_model", std::move(comps)));
    EditSystem system(model_manager);

    EditSystem::SystemHandlerPtr handler { new TrivialEditHandler() };
    HandlerMetaData meta_data;
    meta_data.name = "TrivialEdit";
    meta_data.display_name = "Trivial Edit Handler";
    REQUIRE(system.registerHandler(meta_data, std::move(handler)));
    REQUIRE_NOTHROW(system.unregisterHandler(meta_data));
}
