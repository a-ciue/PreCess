#include "EditSystem.h"
#include "MeshData.h"
#include "ModelData.h"
#include "ModelManager.h"
#include "TrivialEditHandler.h"

#include <catch2/catch_test_macros.hpp>

using namespace systems::edit;

TEST_CASE("EditSystem::register&unregister")
{
    auto mesh_data = std::make_unique<MeshData>();
    mesh_data->init(); 
    auto model = std::make_unique<ModelData>(std::move(mesh_data));
    ModelManager model_manager;
    REQUIRE_NOTHROW(model_manager.addModel(std::move(model)));
    EditSystem system(model_manager);

    EditSystem::SystemHandlerPtr handler { new TrivialEditHandler() };
    HandlerMetaData meta_data;
    meta_data.name = "TrivialEdit";
    meta_data.display_name = "Trivial Edit Handler";
    REQUIRE(system.registerHandler(meta_data, std::move(handler)));
    REQUIRE_NOTHROW(system.unregisterHandler(meta_data));
}
