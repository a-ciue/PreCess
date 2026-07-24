/**
 * @file IgesXdeComponentBuilder.cpp
 * @brief IGES XDE 组件构建器实现
 * @author 范成通
 */
#include "IgesXdeComponentBuilder.h"
#include "GeometryData.h"

#include <TCollection_ExtendedString.hxx>
#include <TDF_Label.hxx>
#include <TDF_LabelSequence.hxx>
#include <TDataStd_Name.hxx>
#include <TDocStd_Document.hxx>
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
    const Handle(XCAFDoc_ShapeTool)& shapeTool,
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

std::optional<ModelPayload> IgesXdeComponentBuilder::buildModelData(
    TDocStd_Document& doc,
    const std::string& modelName)
{
    Handle(XCAFDoc_ShapeTool) shapeTool = XCAFDoc_DocumentTool::ShapeTool(doc.Main());
    if (shapeTool.IsNull()) {
        spdlog::error("Failed to get XCAF shape tool.");
        return std::nullopt;
    }

    ComponentDatas comps;

    TDF_LabelSequence freeShapes;
    shapeTool->GetFreeShapes(freeShapes);

    std::vector<std::pair<TDF_Label, TopoDS_Shape>> leaves;
    for (Standard_Integer i = 1; i <= freeShapes.Length(); ++i) {
        collectLeafShapes(shapeTool, freeShapes.Value(i), leaves);
    }

    if (leaves.empty()) {
        spdlog::warn("[IGES-XDE] no leaf found, fallback to first free shape");

        int freeIndex = 0;
        for (Standard_Integer i = 1; i <= freeShapes.Length(); ++i) {
            TopoDS_Shape s = shapeTool->GetShape(freeShapes.Value(i));
            if (!s.IsNull()) {
                auto geometry_data = std::make_unique<GeometryData>();
                geometry_data->setRootShape(s);

                auto c = std::make_unique<ComponentData>();
                c->id = -1;
                c->name = "Comp_" + std::to_string(freeIndex);
                c->geometry = std::move(geometry_data);

                comps.push_back(std::move(c));
                ++freeIndex;
            }
        }

        if (!comps.empty()) {
            return ModelPayload{modelName, std::move(comps)};
        }

        spdlog::error("[IGES-XDE] no valid shape found");
        return std::nullopt;
    }

    int leafIndex = 0;
    for (const auto& [label, shape] : leaves) {
        auto geometry_data = std::make_unique<GeometryData>();
        geometry_data->setRootShape(shape);

        std::string compName = labelName(label);
        if (compName.empty()) {
            compName = "Comp_" + std::to_string(leafIndex);
        }

        auto c = std::make_unique<ComponentData>();
        c->id = -1;
        c->name = compName;
        c->geometry = std::move(geometry_data);
        c->source_xde_leaf_id = leafIndex;

        comps.push_back(std::move(c));

        spdlog::info("[IGES-XDE] create component: index={}, name='{}', shapeType={}",
            leafIndex, compName, static_cast<int>(shape.ShapeType()));

        ++leafIndex;
    }

    spdlog::info("[IGES-XDE] model '{}' created {} components",
        modelName, comps.size());

    return ModelPayload{modelName, std::move(comps)};
}

}