/**
* @file：Selector.h
* @brief：渲染窗口中的选择和高亮操作
* @author：付轩宇 email 982531420@qq.com

*/
#include <array>
#include <optional>
#include <vector>
#include <vtkNew.h>
#include <vtkSmartPointer.h>
#include <vtkAssembly.h>
#include <vtkPropAssembly.h>

class vtkRenderer;
class vtkProperty;
template <typename T>
class vtkSmartPointer;
class vtkDataSetMapper;
class vtkActor;

namespace Selector {
//! @brief 在renderer中选择在(posx, posy)坐标的actor
//! @return 选中的actor和cell局部索引，没有选中则是返回std::nullopt
std::optional<std::pair<vtkActor*, int>> pick_cell(double posx, double posy, vtkRenderer* renderer);
}
class ActorSelectorHighlight {
public:
    ActorSelectorHighlight(vtkRenderer* renderer);
    ~ActorSelectorHighlight() { clear(); }
    //! @brief 清空selections并取消高亮
    void clear();
    //! @brief 获取当前选中的actors
    std::vector<vtkActor*> get();
    //! @brief 获取当前选中的assembly
    vtkPropAssembly* getAssembly();
  
    //! @brief 找到该坐标下的actor，并高亮该actor；若选中已选actor要取消选中和高亮
    void select(double posx, double posy);

private:
    struct Actor
    {
        vtkSmartPointer<vtkActor> actor;
        vtkSmartPointer<vtkProperty> backup_property;
    };
    //! @brief 存储选中的actor和每个actor原本的颜色渲染设置，用于取消高亮
    std::vector<Actor> selections_;
    vtkRenderer* renderer_;
    vtkPropAssembly* actorassembly;
    //! @brief 取消高亮，修改回原来属性
    static void _cancel_highlight(Actor &selection);
    //! @brief 判断是否已经被选中
    static std::optional<size_t> _is_selected(const vtkActor* new_actor, const std::vector<Actor>& selections);
};

class SingleFaceSelectorHighlight {
public:
    struct SelectedFace {
        //! @brief 面所在的actor，借由actor可以找到全局id
        vtkActor* patch_actor;
        //! @brief 面局部索引id
        int local_id;
    };
    //! @brief 将actor绑定到renderer，mapper绑定到actor
    SingleFaceSelectorHighlight(vtkRenderer* renderer);
    //! @brief 将actor从renderer中删除
    ~SingleFaceSelectorHighlight();
    //! @brief 返回当前选择的面
    std::optional<SelectedFace> get();
    //! @brief 清空selection并取消高亮，即清空mapper
    void clear();
    //! @brief 找到坐标下的face并存储，若选中同一个面需要取消选中。调用Selector::pick_cell()
    void select(double posx, double posy);
    vtkPropAssembly* getAssembly();

private:
    //！@brief 取消高亮，清空mapper
    static void _cancel_highlight(vtkSmartPointer<vtkActor>& selectedActor, vtkRenderer* renderer);
    //! @brief 判断是否已经被选中
    static bool _is_selected(SelectedFace new_face, const std::optional<SelectedFace>& selection, vtkActor* selectedActor);

    vtkRenderer* renderer_;
    std::optional<SelectedFace> selection_;
    //vtkPropAssembly faceAssembly;
    vtkPropAssembly* faceassembly;
    vtkSmartPointer<vtkActor> selectedActor_;
};

class SingleEdgeSelectorHighlight {
public:
    struct SelectedEdge {
        //! @brief 边所在的actor，借由actor可以找到全局id
        vtkActor* actor;
        //! @brief 边端点局部索引id
        std::array<int, 2> v_local_id;
    };
    //! @brief 将actor绑定到renderer，mapper绑定到actor
    SingleEdgeSelectorHighlight(vtkRenderer* renderer);
    //! @brief 将actor从renderer中删除
    ~SingleEdgeSelectorHighlight();
    //! @brief 获取当前选择的边
    std::optional<SelectedEdge> get();
    //! @brief 清空selection并取消高亮，即清空mapper
    void clear();
    //! @brief 找到坐标下的edge并存储，若选中同一个面需要取消选中。调用Selector::pick_cell()
    void select(double posx, double posy);
    vtkPropAssembly* getAssembly();

private:
    //！@brief 取消高亮，清空mapper
    static void _cancel_highlight(vtkDataSetMapper* selectedMapper, vtkActor* selectedActor);
    //! @brief 判断是否已经被选中
    static bool _is_selected(SelectedEdge new_edge, const std::optional<SelectedEdge>& selection, vtkActor* selectedActor);

    vtkRenderer* renderer_;
    std::optional<SelectedEdge> selection_;
    //vtkPropAssembly edgeAssembly;
    vtkPropAssembly* edgeassembly;
    vtkNew<vtkDataSetMapper> selectedMapper_;
    vtkSmartPointer<vtkActor> selectedActor_;
};
