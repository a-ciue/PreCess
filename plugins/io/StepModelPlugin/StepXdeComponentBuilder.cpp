#include "StepXdeComponentBuilder.h"
#include "ComponentData.h"
#include "ModelData.h"
#include "GeometryData.h"

#include <TCollection_AsciiString.hxx>
#include <TDF_Label.hxx>
#include <TDF_LabelSequence.hxx>
#include <TDataStd_Name.hxx>
#include <TDocStd_Document.hxx>
#include <TCollection_ExtendedString.hxx>
#include <cstring>

#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>

#include <TopoDS_Shape.hxx>
#include <spdlog/spdlog.h>

namespace systems::io {

static std::string labelName(const TDF_Label& label)
{
    Handle(TDataStd_Name) nameAttr;
    if (!label.FindAttribute(TDataStd_Name::GetID(), nameAttr)) {
        return {};
    }

    const TCollection_ExtendedString& ext = nameAttr->Get();

    std::string buf(ext.Length() * 4 + 1, '\0');
    char* cstr = buf.data();
    const Standard_Integer n = ext.ToUTF8CString(cstr);
    if (n < 0) {
        return {};
    }

    buf.resize(static_cast<std::string::size_type>(n));
    return buf;
}

static void collectLeafShapes(
    const Handle(XCAFDoc_ShapeTool) & shapeTool,
    const TDF_Label& label,
    std::vector<std::pair<TDF_Label, TopoDS_Shape>>& outLeaves)
{
    TDF_Label refLabel = label;

    if (shapeTool->IsReference(label)) {
        TDF_Label referred;
        if (shapeTool->GetReferredShape(label, referred)) {
            refLabel = referred;
        }
    }

    if (shapeTool->IsAssembly(refLabel)) {
        TDF_LabelSequence children;
        shapeTool->GetComponents(refLabel, children);

        for (Standard_Integer i = 1; i <= children.Length(); ++i) {
            collectLeafShapes(shapeTool, children.Value(i), outLeaves);
        }
        return;
    }

    TopoDS_Shape shape = shapeTool->GetShape(refLabel);
    if (!shape.IsNull()) {
        outLeaves.emplace_back(refLabel, shape);
    }
}

std::unique_ptr<ModelData> StepXdeComponentBuilder::buildModelData(
    TDocStd_Document& doc,
    const std::string& modelName)
{
    Handle(XCAFDoc_ShapeTool) shapeTool = XCAFDoc_DocumentTool::ShapeTool(doc.Main());
    if (shapeTool.IsNull()) {
        spdlog::error("Failed to get XCAF shape tool.");
        return nullptr;
    }

    auto model_data = std::make_unique<ModelData>();
    model_data->model_name_ = modelName;

    TDF_LabelSequence freeShapes;
    shapeTool->GetFreeShapes(freeShapes);

    std::vector<std::pair<TDF_Label, TopoDS_Shape>> leaves;
    for (Standard_Integer i = 1; i <= freeShapes.Length(); ++i) {
        collectLeafShapes(shapeTool, freeShapes.Value(i), leaves);
    }

    if (leaves.empty()) {
        spdlog::warn("[STEP-XDE] no leaf found, fallback to first free shape");

        int freeIndex = 0;
        for (Standard_Integer i = 1; i <= freeShapes.Length(); ++i) {
            TopoDS_Shape s = shapeTool->GetShape(freeShapes.Value(i));
            if (!s.IsNull()) {
                auto geometry_data = std::make_unique<GeometryData>();
                geometry_data->rootShape = std::make_unique<TopoDS_Shape>(s);

                std::string compName = "Comp_" + std::to_string(freeIndex);
                ComponentData* c = model_data->createComponent(-1, compName);
                c->geometry = std::move(geometry_data);

                ++freeIndex;
            }
        }

        if (!model_data->componentDatas().empty()) {
            return model_data;
        }

        spdlog::error("[STEP-XDE] no valid shape found");
        return nullptr;
    }

    int leafIndex = 0;
    for (const auto& [label, shape] : leaves) {
        auto geometry_data = std::make_unique<GeometryData>();
        geometry_data->rootShape = std::make_unique<TopoDS_Shape>(shape);

        std::string compName = labelName(label);
        if (compName.empty()) {
            compName = "Comp_" + std::to_string(leafIndex);
        }

        ComponentData* c = model_data->createComponent(-1, compName);
        c->geometry = std::move(geometry_data);
        c->source_xde_leaf_id = leafIndex;

        spdlog::info("[STEP-XDE] create component: index={}, name='{}', shapeType={}",
            leafIndex, compName, static_cast<int>(shape.ShapeType()));

        ++leafIndex;
    }

    spdlog::info("[STEP-XDE] model '{}' created {} components",
        model_data->model_name_, model_data->componentDatas().size());

    return model_data;
}
}