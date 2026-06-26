/**
* @file：QRenderWindowStyle.h
* @brief：渲染窗口中的交互操作
* @author：付轩宇 982531420@qq.com

*/
#ifndef Q_RENDER_WINDOW_STYLE_H
#define Q_RENDER_WINDOW_STYLE_H
#include <vtkInteractorStyleTrackballCamera.h>

class SelectManager;
class GeometrySelectManager;


class QRenderWindowStyle : public vtkInteractorStyleTrackballCamera {
public:
	static QRenderWindowStyle* New();
    vtkTypeMacro(QRenderWindowStyle, vtkInteractorStyleTrackballCamera);
    void SetClick();
    void SetSelectManager(SelectManager* select_manager);
    void SetGeometrySelectManager(GeometrySelectManager* geometry_select_manager);
    void OnLeftButtonDown() override;
    void OnMouseMove() override;
    void OnLeftButtonUp() override;
     
private:    
    bool click_ {};
    bool trackball_started_ { false };
    bool button_down_ { false };
    int downPos_[2] { 0, 0 };
    SelectManager* select_manager_{};
    GeometrySelectManager* geometry_select_manager_{};

};

//class MouseInteractorHighLightActor : public vtkInteractorStyleWithClick {
//public:
//    static MouseInteractorHighLightActor* New();
//    vtkTypeMacro(MouseInteractorHighLightActor,
//        vtkInteractorStyleTrackballCamera);
//
//    virtual ~MouseInteractorHighLightActor() { }
//    void OnLeftButtonUp() override;
//    /*
//    * @brief 调用select
//    
//
//    */
//    void OnSelect(double posx, double posy);
//
//    void SetClick() override;
//    void ClearSelections() override;
//    /*
//    * @brief 获取选中的Selection
//    *
//    * 判断选中的对象所属的模型，将全局ID转化为ModelActor中的局部ID，并存入Selection
//    * @param[in] mActors 所有模型与模型名的映射
//    * @param[in] mode 当前的选择模式
//
//    */
//    std::unique_ptr<Selection> GetSelectedIDs(const std::unordered_map<QString, std::unique_ptr<MeshActor>>& mActors, SelectMode mode) override;
//
//    void SetSelector(std::unique_ptr<BlockSelectorHighlight> selector);
//    
//private:
//    bool click_ {};
//    std::unique_ptr<BlockSelectorHighlight> selector_;
//};
//
//class MouseInteractorHighLightFace : public vtkInteractorStyleWithClick {
//public:
//    static MouseInteractorHighLightFace* New();
//    vtkTypeMacro(MouseInteractorHighLightFace,
//        vtkInteractorStyleTrackballCamera);
//
//    virtual ~MouseInteractorHighLightFace() { }
//    virtual void OnLeftButtonUp() override;
//
//    
//    void SetClick() override;
//    void ClearSelections() override;
//    void SetSelector(std::unique_ptr<SingleFaceSelectorHighlight> selector);
//    /*
//   * @brief 获取选中的Selection
//   *
//   * 判断选中的对象所属的模型，将全局ID转化为ModelActor中的局部ID，并存入Selection
//   * @param[in] mActors 所有模型与模型名的映射
//   * @param[in] mode 当前的选择模式
//
//   */
//    std::unique_ptr<Selection> GetSelectedIDs(const std::unordered_map<QString, std::unique_ptr<MeshActor>>& mActors, SelectMode mode) override;
//
//private:
//   
//    bool click_ {};
//    std::unique_ptr<SingleFaceSelectorHighlight> selector_;
//};
//
//class MouseInteractorHighLightEdge : public vtkInteractorStyleWithClick {
//public:
//    static MouseInteractorHighLightEdge* New();
//    vtkTypeMacro(MouseInteractorHighLightEdge,
//        vtkInteractorStyleTrackballCamera);
//
//    virtual ~MouseInteractorHighLightEdge() { }
//    virtual void OnLeftButtonUp() override;
//
//    
//
//    void SetClick() override;
//    void ClearSelections() override;
//    void SetSelector(std::unique_ptr<SingleEdgeSelectorHighlight> selector);
//    /*
//   * @brief 获取选中的Selection
//   *
//   * 判断选中的对象所属的模型，将全局ID转化为ModelActor中的局部ID，并存入Selection
//   * @param[in] mActors 所有模型与模型名的映射
//   * @param[in] mode 当前的选择模式
//
//   */
//    std::unique_ptr<Selection> GetSelectedIDs(const std::unordered_map<QString, std::unique_ptr<MeshActor>>& mActors, SelectMode mode) override;
//private:
//    
//    bool click_ {};
//    std::unique_ptr<SingleEdgeSelectorHighlight> selector_;
//};
#endif // STYLE_H
