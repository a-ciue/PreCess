#ifndef STYLE_H
#define STYLE_H
#include <unordered_map>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkObjectFactory.h>
#include <vtkSmartPointer.h>

#include "Selector.h"

enum class SelectMode;
class vtkProperty;
class vtkNamedColors;
class ModelActor;

class vtkInteractorStyleWithClick : public vtkInteractorStyleTrackballCamera {
public:
    virtual void SetClick() = 0;
    virtual void ClearSelections() = 0;
    virtual std::vector<int> GetSelectedIDs(const std::vector<ModelActor*> &mActors, SelectMode  mode) = 0;
};

class MouseInteractorHighLightActor : public vtkInteractorStyleWithClick {
public:
    static MouseInteractorHighLightActor* New();
    vtkTypeMacro(MouseInteractorHighLightActor,
        vtkInteractorStyleTrackballCamera);

    virtual ~MouseInteractorHighLightActor() { }
    void OnLeftButtonUp() override;

    void OnSelect(double posx, double posy);

    void SetClick() override;
    void ClearSelections() override;
    std::vector<int> GetSelectedIDs(const std::vector<ModelActor*>& mActors, SelectMode mode) override;

    void SetSelector(std::unique_ptr<ActorSelectorHighlight> selector);
    
private:
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

    
    void SetClick() override;
    void ClearSelections() override;
    void SetSelector(std::unique_ptr<SingleFaceSelectorHighlight> selector);
    std::vector<int> GetSelectedIDs(const std::vector<ModelActor*>& mActors, SelectMode mode) override;

private:
   
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

    

    void SetClick() override;
    void ClearSelections() override;
    void SetSelector(std::unique_ptr<SingleEdgeSelectorHighlight> selector);
    std::vector<int> GetSelectedIDs(const std::vector<ModelActor*>& mActors, SelectMode mode) override;
private:
    
    bool click_ {};
    std::unique_ptr<SingleEdgeSelectorHighlight> selector_;
};
#endif // STYLE_H
