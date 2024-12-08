#include <unordered_map>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkObjectFactory.h>
#include <vtkSmartPointer.h>

#include "Selector.h"

class vtkProperty;
class vtkNamedColors;
class Model;

class vtkInteractorStyleWithClick : public vtkInteractorStyleTrackballCamera {
public:
    virtual void SetClick() = 0;
    virtual void ClearSelections() = 0;
};

class MouseInteractorHighLightActor : public vtkInteractorStyleWithClick {
public:
    static MouseInteractorHighLightActor* New();
    vtkTypeMacro(MouseInteractorHighLightActor,
        vtkInteractorStyleTrackballCamera);

    virtual ~MouseInteractorHighLightActor() { }
    void OnLeftButtonUp() override;

    void OnSelect(double posx, double posy);
    void OnCommitMergeBlocks();
    void OnCommitRemeshBlocks();
    void OnCommitMergeGroups();
    void OnCommitRemeshGroups();

    void SetModel(Model* model);
    void SetClick() override;
    void ClearSelections() override;
    void SetSelector(std::unique_ptr<ActorSelectorHighlight> selector);

private:
    Model* model_;
    bool click_ {};
    std::unique_ptr<ActorSelectorHighlight> selector_;
};

class MouseInteractorHighLightFace : public vtkInteractorStyleWithClick {
public:
    static MouseInteractorHighLightFace* New();
    vtkTypeMacro(MouseInteractorHighLightFace,
        vtkInteractorStyleTrackballCamera);

    virtual ~MouseInteractorHighLightFace() { }
    virtual void OnLeftButtonUp() override;

    void OnCommitSplitFace();

    void SetModel(Model* model);
    void SetClick() override;
    void ClearSelections() override;
    void SetSelector(std::unique_ptr<SingleFaceSelectorHighlight> selector);

private:
    Model* model_;
    bool click_ {};
    std::unique_ptr<SingleFaceSelectorHighlight> selector_;
};

class MouseInteractorHighLightEdge : public vtkInteractorStyleWithClick {
public:
    static MouseInteractorHighLightEdge* New();
    vtkTypeMacro(MouseInteractorHighLightEdge,
        vtkInteractorStyleTrackballCamera);

    virtual ~MouseInteractorHighLightEdge() { }
    virtual void OnLeftButtonUp() override;

    void OnCommitSplitEdge();

    void SetModel(Model* model);
    void SetClick() override;
    void ClearSelections() override;
    void SetSelector(std::unique_ptr<SingleEdgeSelectorHighlight> selector);

private:
    Model* model_;
    bool click_ {};
    std::unique_ptr<SingleEdgeSelectorHighlight> selector_;
};
