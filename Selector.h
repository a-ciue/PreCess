/**
* @file：Selector.h
* @brief：渲染窗口中的选择和高亮操作
* @author：付轩宇 email 982531420@qq.com

*/
#ifndef SELECTOR_H
#define SELECTOR_H
#include <array>
#include <optional>
#include <vector>
#include <vtkNew.h>
#include <vtkSmartPointer.h>
#include <vtkAssembly.h>
#include <vtkPropAssembly.h>
#include "Selection.h"
#include <vtkCompositeDataDisplayAttributes.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkCompositePolyDataMapper.h>
#include "Core.h"

class vtkRenderer;
class vtkProperty;
template <typename T>
class vtkSmartPointer;
class vtkDataSetMapper;
class vtkActor;
class Model;

using SelectionVtk = Selection;
//namespace Selector {
////! @brief 在renderer中选择在(posx, posy)坐标的actor
////! @return 选中的actor和cell局部索引，没有选中则是返回std::nullopt
//std::optional<std::pair<vtkActor*, int>> pick_cell(double posx, double posy, vtkRenderer* renderer);
//}

class SelectorHighlight {
public:
    virtual void select(double posx, double posy) = 0;
    virtual void clear() = 0;
    virtual SelectionVtk get()=0;
    virtual vtkPropCollection* getPickList()=0;
};

class BlockSelectorHighlight : public SelectorHighlight {
public:
    BlockSelectorHighlight(vtkRenderer* renderer);
    ~BlockSelectorHighlight() { clear(); }
    //! @brief 清空selections并取消高亮
    void clear();
    //! @brief 获取当前选中的actors
    SelectionVtk get();
    //! @brief 找到该坐标下的actor，并高亮该actor；若选中已选actor要取消选中和高亮
    void select(double posx, double posy,vtkRenderer* renderer);

private:
    /* struct Actor
    {
        vtkSmartPointer<vtkActor> actor;
        vtkSmartPointer<vtkProperty> backup_property;
    };*/
    //! @brief 存储选中的actor和每个actor原本的颜色渲染设置，用于取消高亮
    struct Block{
        vtkIdType block_id;
        double backup_color[3]{};
    };
    std::vector<Block> selections_;
    vtkRenderer* renderer_;
    vtkCompositePolyDataMapper* mapper_;
    vtkPropCollection* collection;
    void _cancel_highlight(Block &selection);
    static std::optional<size_t> _is_selected(const vtkIdType block_id, const std::vector<Block>& selections);
};

class SingleFaceSelectorHighlight : public SelectorHighlight {
public:
    //struct SelectedFace {
    //    //! @brief 面所在的actor，借由actor可以找到全局id
    //    vtkIdType face_id;
    //    //! @brief 面局部索引id
    //    int local_id;
    //};
    //! @brief 将actor绑定到renderer，mapper绑定到actor
    SingleFaceSelectorHighlight(vtkRenderer* renderer);
    //! @brief 将actor从renderer中删除
    ~SingleFaceSelectorHighlight();
    //! @brief 返回当前选择的面
    SelectionVtk get();
    //! @brief 清空selection并取消高亮，即清空mapper
    void clear();
    //! @brief 找到坐标下的face并存储，若选中同一个面需要取消选中。调用Selector::pick_cell()
    void select(double posx, double posy, vtkRenderer* renderer);
    

private:
    //！@brief 取消高亮，清空mapper
    void _cancel_highlight(std::optional<vtkIdType> selection, vtkRenderer* renderer);
    //! @brief 判断是否已经被选中
    static bool _is_selected(vtkIdType new_face_id, const std::optional<vtkIdType>& selection);

    void set_highlight_actor();
    vtkRenderer* renderer_;
    std::optional<vtkIdType> selection_;
    vtkNew<vtkActor> highlight_actor_;
    vtkNew<vtkMapper> mapper_;
};

class SingleEdgeSelectorHighlight : public SelectorHighlight {
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
    SelectionVtk get();
    //! @brief 清空selection并取消高亮，即清空mapper
    void clear();
    //! @brief 找到坐标下的edge并存储，若选中同一个面需要取消选中。调用Selector::pick_cell()
    void select(double posx, double posy);

private:
    //！@brief 取消高亮，清空mapper
    static void _cancel_highlight(vtkDataSetMapper* selectedMapper, vtkActor* selectedActor);
    //! @brief 判断是否已经被选中
    static bool _is_selected(SelectedEdge new_edge, const std::optional<SelectedEdge>& selection, vtkActor* selectedActor);

    vtkRenderer* renderer_;
    Model* model_;
    std::optional<SelectedEdge> selection_;
    //vtkPropAssembly edgeAssembly;
    vtkNew<vtkMapper> mapper_;
    vtkNew<vtkDataSetMapper> selectedMapper_;
    vtkSmartPointer<vtkActor> selectedActor_;
};
#endif