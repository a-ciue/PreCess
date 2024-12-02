#include <unordered_map>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkSmartPointer.h>

#include "Selector.h"

class vtkProperty;
class vtkNamedColors;
class Model;

class MouseInterActorHighLightActor : public vtkInteractorStyleTrackballCamera {
public:
    static MouseInterActorHighLightActor* New();
    vtkTypeMacro(MouseInterActorHighLightActor,
        vtkInteractorStyleTrackballCamera);

    virtual ~MouseInterActorHighLightActor() { }
    virtual void OnLeftButtonDown() override;
    virtual void OnKeyPress() override;

    void SetDefaultRenderer(vtkRenderer*) override;
    void set_model(Model* model);

private:
    Model* model;
    ActorSelectorHighlight selector_;

    void UnselectActor(vtkSmartPointer<vtkActor> actor);
    void SelectActor(vtkSmartPointer<vtkActor> actor);
};

class MouseInteractorHighLightFace : public vtkInteractorStyleTrackballCamera {
public:
    static MouseInteractorHighLightFace* New();
    vtkTypeMacro(MouseInteractorHighLightFace,
        vtkInteractorStyleTrackballCamera);

    virtual ~MouseInteractorHighLightFace() { }
    virtual void OnLeftButtonDown() override;
    virtual void OnKeyPress() override;
    void SetActorMap(std::unordered_map<vtkActor*, int>* actorMap);
    void SetPatchList(MeshLib::PatchList* Mesh);

private:
    vtkSmartPointer<vtkActor> pickedActors[2];
    vtkSmartPointer<vtkProperty> savedProperties[2];
    MeshLib::PatchList* Mesh;
    std::unordered_map<vtkActor*, int>* actorMap;
    vtkNew<vtkNamedColors> colors;

    // 记录已选actor的数量
    int numPickedActors = 0;

    void UnselectActor(vtkSmartPointer<vtkActor> actor);
    void SelectActor(vtkSmartPointer<vtkActor> actor);
};

class MouseInteractorHighLightEdge : public vtkInteractorStyleTrackballCamera {
public:
    static MouseInteractorHighLightEdge* New();
    vtkTypeMacro(MouseInteractorHighLightEdge,
        vtkInteractorStyleTrackballCamera);

    virtual ~MouseInteractorHighLightEdge() { }
    virtual void OnLeftButtonDown() override;
    virtual void OnKeyPress() override;
    void SetActorMap(std::unordered_map<vtkActor*, int>* actorMap);
    void SetPatchList(MeshLib::PatchList* Mesh);

private:
    vtkSmartPointer<vtkActor> pickedActors[2];
    vtkSmartPointer<vtkProperty> savedProperties[2];
    MeshLib::PatchList* Mesh;
    std::unordered_map<vtkActor*, int>* actorMap;
    vtkNew<vtkNamedColors> colors;

    // 记录已选actor的数量
    int numPickedActors = 0;

    void UnselectActor(vtkSmartPointer<vtkActor> actor);
    void SelectActor(vtkSmartPointer<vtkActor> actor);
};
