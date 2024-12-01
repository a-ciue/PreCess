#include <vector>
#include <optional>

class vtkRenderer;
class vtkProperty;
template <typename T>
class vtkSmartPointer;
class vtkDataSetMapper;
class vtkActor;
class Model;

namespace Selector {
int select_actor(double posx, double posy, vtkRenderer* renderer);
int select_cell(double posx, double posy, vtkRenderer* renderer);
}
class ActorSelectorHighlight {
public:
    ActorSelectorHighlight();
    ~ActorSelectorHighlight() { _cancel_highlight(); }
    void clear();
    std::vector<int> get();
    int select(double posx, double posy);

    void set_renderer(vtkRenderer* renderer);

protected:
    void add_selection(int selected, vtkProperty* selected_prop);

private:
    std::vector<std::pair<int, vtkSmartPointer<vtkProperty>>> selections;
    void _cancel_highlight();
};

class SingleFaceSelectorHighlight {
public:
    SingleFaceSelectorHighlight();
    ~SingleFaceSelectorHighlight() { _cancel_highlight(); };
    std::optional<int> get();
    void clear();
    int select(double posx, double posy);

    void set_model(Model* model);
    void set_renderer(vtkRenderer* renderer);

private:
    void _cancel_highlight();

    Model* model;
    vtkRenderer* renderer;
    std::optional<int> selected;
    vtkSmartPointer<vtkDataSetMapper> selectedMapper;
    vtkSmartPointer<vtkActor> selectedActor;
};

class SingleEdgeSelectorHighlight {
public:
    SingleEdgeSelectorHighlight();
    ~SingleEdgeSelectorHighlight() { _cancel_highlight(); };
    std::optional<pair<int,int>> get();
    void clear();
    int select(double posx, double posy);

    void set_model(Model* model);
    void set_renderer(vtkRenderer* renderer);

private:
    void _cancel_highlight();

    Model* model;
    vtkRenderer* renderer;
    std::optional<pait<int,int>> selected;
    vtkSmartPointer<vtkDataSetMapper> selectedMapper;
    vtkSmartPointer<vtkActor> selectedActor;
};
