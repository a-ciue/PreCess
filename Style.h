/**
* @file：Style.h
* @brief：渲染窗口中的交互操作
* @author：付轩宇 982531420@qq.com

*/
#ifndef STYLE_H
#define STYLE_H
#include <unordered_map>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkObjectFactory.h>
#include <vtkSmartPointer.h>
#include "Selection.h"
#include "Selector.h"

enum class SelectMode;
class vtkProperty;
class vtkNamedColors;
class ModelActor;

class vtkInteractorStyleWithClick : public vtkInteractorStyleTrackballCamera {
public:
    virtual void SetClick() = 0;
    virtual void ClearSelections() = 0;
    /*
    * @brief 虚函数获取选中actorid
    *
    * @param[in] mActors 所有模型与模型名的映射
    * @param[in] mode 当前的选择模式

    */
    virtual std::unique_ptr<Selection> GetSelectedIDs(const std::unordered_map<QString, std::unique_ptr<ModelActor>>& mActors, SelectMode  mode) = 0;
};

class MouseInteractorHighLightActor : public vtkInteractorStyleWithClick {
public:
    static MouseInteractorHighLightActor* New();
    vtkTypeMacro(MouseInteractorHighLightActor,
        vtkInteractorStyleTrackballCamera);

    virtual ~MouseInteractorHighLightActor() { }
    void OnLeftButtonUp() override;
    /*
    * @brief 调用select
    

    */
    void OnSelect(double posx, double posy);

    void SetClick() override;
    void ClearSelections() override;
    /*
    * @brief 获取选中的Selection
    *
    * 判断选中的对象所属的模型，将全局ID转化为ModelActor中的局部ID，并存入Selection
    * @param[in] mActors 所有模型与模型名的映射
    * @param[in] mode 当前的选择模式

    */
    std::unique_ptr<Selection> GetSelectedIDs(const std::unordered_map<QString, std::unique_ptr<ModelActor>>& mActors, SelectMode mode) override;

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
    /*
   * @brief 获取选中的Selection
   *
   * 判断选中的对象所属的模型，将全局ID转化为ModelActor中的局部ID，并存入Selection
   * @param[in] mActors 所有模型与模型名的映射
   * @param[in] mode 当前的选择模式

   */
    std::unique_ptr<Selection> GetSelectedIDs(const std::unordered_map<QString, std::unique_ptr<ModelActor>>& mActors, SelectMode mode) override;

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
    /*
   * @brief 获取选中的Selection
   *
   * 判断选中的对象所属的模型，将全局ID转化为ModelActor中的局部ID，并存入Selection
   * @param[in] mActors 所有模型与模型名的映射
   * @param[in] mode 当前的选择模式

   */
    std::unique_ptr<Selection> GetSelectedIDs(const std::unordered_map<QString, std::unique_ptr<ModelActor>>& mActors, SelectMode mode) override;
private:
    
    bool click_ {};
    std::unique_ptr<SingleEdgeSelectorHighlight> selector_;
};
#endif // STYLE_H
