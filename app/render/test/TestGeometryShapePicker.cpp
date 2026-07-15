#include "GeometryActor.h"
#include "GeometryActorManager.h"
#include "GeometrySelectManager.h"
#include "GeometryActorManagerSelectOp.h"

#include "GeometryRegistry.h"
#include "GeometrySubshapeIndex.h"
#include "Selection.h"

#include <IVtkOCC_Shape.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS_Shape.hxx>

#include <BRep_Builder.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <TDF_LabelSequence.hxx>
#include <TDocStd_Document.hxx>
#include <TopoDS_Compound.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>

#include <vtkActor.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

static std::optional<TopoDS_Shape> loadStepAsSingleShape_XDE(const std::filesystem::path& path)
{
    Handle(TDocStd_Document) doc;
    Handle(XCAFApp_Application)::DownCast(XCAFApp_Application::GetApplication())
        ->NewDocument("MDTV-XCAF", doc);

    STEPCAFControl_Reader reader;
    IFSelect_ReturnStatus stat = reader.ReadFile(path.string().c_str());
    if (stat != IFSelect_RetDone) {
        spdlog::error("Failed to read STEP file: {}", path.string());
        return std::nullopt;
    }
    if (!reader.Transfer(doc)) {
        spdlog::error("Failed to transfer STEP file: {}", path.string());
        return std::nullopt;
    }

    Handle(XCAFDoc_ShapeTool) shapeTool = XCAFDoc_DocumentTool::ShapeTool(doc->Main());
    if (shapeTool.IsNull()) {
        spdlog::error("Failed to get XCAF shape tool");
        return std::nullopt;
    }

    TDF_LabelSequence freeShapes;
    shapeTool->GetFreeShapes(freeShapes);
    if (freeShapes.Length() <= 0) {
        spdlog::error("No free shapes in STEP: {}", path.string());
        return std::nullopt;
    }

    BRep_Builder builder;
    TopoDS_Compound comp;
    builder.MakeCompound(comp);

    int added = 0;
    for (Standard_Integer i = 1; i <= freeShapes.Length(); ++i) {
        TopoDS_Shape s = shapeTool->GetShape(freeShapes.Value(i));
        if (s.IsNull())
            continue;
        builder.Add(comp, s);
        ++added;
    }

    if (added == 0) {
        spdlog::error("All free shapes are null in STEP: {}", path.string());
        return std::nullopt;
    }

    spdlog::info("STEP loaded: {}, freeShapes={}, merged={}", path.string(), freeShapes.Length(), added);
    return TopoDS_Shape(comp);
}

class GeometrySelectInteractorStyle : public vtkInteractorStyleTrackballCamera {
public:
    static GeometrySelectInteractorStyle* New();
    vtkTypeMacro(GeometrySelectInteractorStyle, vtkInteractorStyleTrackballCamera);

    void SetSelectManager(GeometrySelectManager* m) { mgr_ = m; }

    void OnLeftButtonDown() override
    {
        button_down_ = true;
        click_ = true;
        trackball_started_ = false;
        this->GetInteractor()->GetEventPosition(downPos_);
    }

    void OnMouseMove() override
    {
        if (button_down_ && click_) {
            int cur[2];
            this->GetInteractor()->GetEventPosition(cur);
            const int dx = cur[0] - downPos_[0];
            const int dy = cur[1] - downPos_[1];
            if (dx * dx + dy * dy > 25) {
                click_ = false;
                if (!trackball_started_) {
                    trackball_started_ = true;
                    vtkInteractorStyleTrackballCamera::OnLeftButtonDown();
                }
            }
        }
        vtkInteractorStyleTrackballCamera::OnMouseMove();
    }

    void OnLeftButtonUp() override
    {
        button_down_ = false;
        if (click_ && mgr_) {
            click_ = false;
            int pos[2];
            this->GetInteractor()->GetEventPosition(pos);
            mgr_->select(pos[0], pos[1]);
            return;
        }
        vtkInteractorStyleTrackballCamera::OnLeftButtonUp();
    }

    void OnKeyPress() override
    {
        const std::string key = this->GetInteractor()->GetKeySym()
            ? this->GetInteractor()->GetKeySym()
            : "";

        if (!mgr_) {
            vtkInteractorStyleTrackballCamera::OnKeyPress();
            return;
        }

        if (key == "1") {
            mgr_->setSelectMode(SelectMode::GeometryVertex);
            spdlog::info("[GEOMETRY] mode=Vertex");
            return;
        }
        if (key == "2") {
            mgr_->setSelectMode(SelectMode::GeometryEdge);
            spdlog::info("[GEOMETRY] mode=Edge");
            return;
        }
        if (key == "3") {
            mgr_->setSelectMode(SelectMode::GeometryFace);
            spdlog::info("[GEOMETRY] mode=Face");
            return;
        }
        if (key == "4") {
            mgr_->setSelectMode(SelectMode::GeometrySolid);
            spdlog::info("[GEOMETRY] mode=Solid");
            return;
        }
        if (key == "c" || key == "C") {
            mgr_->clearSelection();
            spdlog::info("[GEOMETRY] clear selection");
            return;
        }

        vtkInteractorStyleTrackballCamera::OnKeyPress();
    }

private:
    bool click_ = false;
    bool trackball_started_ = false;
    bool button_down_ = false;
    int downPos_[2] { 0, 0 };
    GeometrySelectManager* mgr_ = nullptr;
};

vtkStandardNewMacro(GeometrySelectInteractorStyle);

} // namespace

int main(int argc, char** argv)
{
    std::optional<std::filesystem::path> stepPath;
    std::string modeStr = "face";

    if (argc >= 2) {
        std::filesystem::path p = argv[1];
        if (std::filesystem::exists(p)) {
            stepPath = p;
            if (argc >= 3)
                modeStr = argv[2];
        } else {
            modeStr = argv[1];
        }
        std::transform(modeStr.begin(), modeStr.end(), modeStr.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (modeStr != "face" && modeStr != "edge" && modeStr != "vertex" && modeStr != "solid")
            modeStr = "face";
    }

    SelectMode mode = SelectMode::GeometryFace;
    if (modeStr == "edge")
        mode = SelectMode::GeometryEdge;
    else if (modeStr == "vertex")
        mode = SelectMode::GeometryVertex;
    else if (modeStr == "solid")
        mode = SelectMode::GeometrySolid;

    TopoDS_Shape root;
    if (stepPath) {
        auto s = loadStepAsSingleShape_XDE(*stepPath);
        if (!s)
            return 1;
        root = *s;
    } else {
        root = BRepPrimAPI_MakeBox(100.0, 80.0, 60.0).Shape();
    }

    GeometryRegistry reg;
    GeometrySubshapeIndex cadIndex;
    cadIndex.build(root, reg);

    vtkNew<vtkRenderer> renderer;
    renderer->SetBackground(0.2, 0.3, 0.4);

    vtkNew<vtkRenderWindow> rw;
    rw->AddRenderer(renderer);
    rw->SetSize(900, 700);

    vtkNew<vtkRenderWindowInteractor> iren;
    iren->SetRenderWindow(rw);

    GeometryActorManager gmgr;
    gmgr.bindRender(renderer);

    const Index component_id = 123;
    GeometryDataVtk gd { root, component_id, &cadIndex };
    gmgr.loadGeometry(gd);

    auto ga = gmgr.getComponentActor(component_id);
    if (!ga) {
        spdlog::error("GeometryActor not found for component_id={}", component_id);
        return 1;
    }

    gmgr.setRenderEdge(component_id, true);
    gmgr.setVisibility(component_id, true);

    vtkNew<vtkActor> highlightActor;
    highlightActor->PickableOff();
    renderer->AddActor(highlightActor);

    GeometrySelectManager selMgr(*renderer, *highlightActor, gmgr.op());

    selMgr.setSelectMode(mode);

    vtkNew<GeometrySelectInteractorStyle> style;
    style->SetDefaultRenderer(renderer);
    style->SetSelectManager(&selMgr);

    iren->SetInteractorStyle(style);

    renderer->ResetCamera();
    rw->Render();

    spdlog::info("GEOMETRY selector hotkeys: [1]=Vertex [2]=Edge [3]=Face [4]=Solid [C]=Clear");

    iren->Start();
    return 0;
}
