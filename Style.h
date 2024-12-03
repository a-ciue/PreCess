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
    virtual void OnLeftButtonUp() override;

    void OnCommitMergeBlocks();
    void OnCommitMergeGroups();

    void SetModel(Model* model);
    void SetSelector(std::unique_ptr<ActorSelectorHighlight> selector);

private:
    Model* model;
    std::unique_ptr<ActorSelectorHighlight> selector_;
};

class MouseInteractorHighLightFace : public vtkInteractorStyleTrackballCamera {
public:
    static MouseInteractorHighLightFace* New();
    vtkTypeMacro(MouseInteractorHighLightFace,
        vtkInteractorStyleTrackballCamera);

    virtual ~MouseInteractorHighLightFace() { }
    virtual void OnLeftButtonUp() override;

	void OnCommitSplitFace();

    void SetModel(Model* model);
    void SetSelector(std::unique_ptr<SingleFaceSelectorHighlight> selector);

private:
    Model* model;
    std::unique_ptr<SingleFaceSelectorHighlight> selector_;
};

class MouseInteractorHighLightEdge : public vtkInteractorStyleTrackballCamera {
public:
    static MouseInteractorHighLightEdge* New();
    vtkTypeMacro(MouseInteractorHighLightEdge,
        vtkInteractorStyleTrackballCamera);

    virtual ~MouseInteractorHighLightEdge() { }
    virtual void OnLeftButtonUp() override;

	void OnCommitSplitEdge();

    void SetModel(Model* model);
    void SetSelector(std::unique_ptr<SingleEdgeSelectorHighlight> selector);

private:
    Model* model;
    std::unique_ptr<SingleEdgeSelectorHighlight> selector_;
};
