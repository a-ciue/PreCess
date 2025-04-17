/**
* @file：MyVtkItem.h
* @brief：定义渲染窗口，以及渲染窗口中的操作
* @author：付轩宇 email 982531420@qq.com

*/


#ifndef MYVTKITEM_H
#define MYVTKITEM_H
#include <QQuickVTKItem.h>
#include <QVTKRenderWindowAdapter.h>

#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkCapsuleSource.h>
#include <vtkConeSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkRendererCollection.h>

#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtQml/qqmlregistration.h>
#include <vtkSphereSource.h>
#include <vtkOBJReader.h>

#include "Model.h"
#include "Style.h"
#include "Selection.h"
#include "ModelActor.h"
#include "SelectManager.h"
enum class SelectMode { Group, Block, Face, Edge }; 

struct QRenderWindow : QQuickVTKItem {            //结构体继承QQuickVTKItem
    Q_OBJECT
    Q_PROPERTY(QSelection* selectedIDs READ selectedIDs NOTIFY selectedChanged)
    QML_ELEMENT
public:
    QRenderWindow();                              //槽函数，改变边框重置相机

    struct Data : vtkObject {                 //结构体继承vtkObject
        static Data* New();
        vtkTypeMacro(Data, vtkObject);


        vtkNew<vtkRenderer> renderer_;

        std::unordered_map<QString, std::unique_ptr<ModelActor>> models_;
        vtkNew<vtkInteractorStyleWithClick> style_;


    };

    vtkUserData initializeVTK(vtkRenderWindow* renderWindow) override;
    void destroyingVTK(vtkRenderWindow* renderWindow, vtkUserData userData) override;

    Q_INVOKABLE void resetCamera();
    //void dispatchChangedSource();

    QSelection* selectedIDs();
    Q_INVOKABLE void setSelectModel(QString model_name);
    Q_INVOKABLE void setSelectMode(SelectMode select_mode);
    Q_INVOKABLE void clearSelection();

    Q_INVOKABLE void setRenderMode(ModelActor::RenderMode render_mode);
    Q_INVOKABLE void setEdgeRender(bool is_render);
    Q_INVOKABLE void setVisibility(QString model_name, bool visibility);
    Q_INVOKABLE void setModelData(QString model_name, ModelData model_data);
    
    Q_INVOKABLE void createModel(QString model_name);
    Q_INVOKABLE void renameModel(QString old_name, QString new_name);
    Q_INVOKABLE void deleteModel(QString mode_name);


   /* void onModelInited(QString model_name,const std::unordered_map<int, std::unique_ptr<Patch>>* patches,
        const std::unordered_map<int, std::unique_ptr<Block>>* blocks,
        const std::unordered_map<int, std::unique_ptr<Group>>* groups);
 */
    Q_SLOT void setClick();

    // Q_PROPERTY(QString file READ file WRITE setFile NOTIFY fileChanged)
    //QString source() const { return _source; }
    //void setSource(QString v);

    bool event(QEvent* ev) override;

signals:
    void selectedChanged();
    void clicked();
 
private:

    
    bool edge_render_;
    ModelActor::RenderMode renderMode_{};
    SelectMode select_mode_ {};


    vtkNew<vtkCamera> _camera;
    
    std::unique_ptr <SelectManager> selectManager_;
    ModelActor* cur_actor_{};
    QString cur_actor_name_;
    vtkInteractorStyleWithClick* cur_style_{};

    std::unique_ptr<QMouseEvent> _click;
    const Data* data_{};
   
 };
#endif 
 
  /* Q_INVOKABLE void bindStyle(QString function);*/
 //Q_INVOKABLE void unbindStyle();
    ///*
    //* @brief 对模型进行边渲染
    //*
    //* 将render中指定的模型的actor调用render_edge（将edge设为可见）
    //* @param[in] model_name 模型名
    //* @param[in] renderMode 当前的渲染模式
    //* @param[in] render

    //*/
    //Q_INVOKABLE void changeEdgeRender(QString model_name,QString renderMode, bool render);
    ///*
    //* @brief 对渲染窗口中的actor_进行初始化
    //*
    //* 创建actor_[model_name]并进行初始化
    //* @param[in] model_name 模型名
    //* @param[in] patches 模型层patch数据
    //* @param[in] blocks 模型层block数据
    //* @param[in] groups 模型层group数据

    //*/
   /*
    * @brief 合并块操作
    *
    * 调用ModelActor的merge_blocks函数
    * @param[in] model_name 模型名
    * @param block_ids 要合并的block
    * @param father_block 留下的block
    * @param father_block_patches 留下的block合并后拥有的patches id

    */
 
    //Q_INVOKABLE void deleteModel(QString model_name);
    ///*
    //*@brief 模型改名
    //*
    //* actor_中删除旧的映射重新加入
    //* 
    //* param[in] old name 旧模型名
    //* param[in] new name 新模型名
    //*/
    //Q_INVOKABLE void renameModel(QString old_name, QString new_name);
    ///*
    //*@brief 设置渲染窗口中模型可见性
    //*
    //* 设置渲染窗口中模型assembly的setVisibility
    //* param[in] model_name 模型名
    //* param[in] visibility 可见度
    //*/
    /*Q_INVOKABLE void setVisibility(QString model_name, bool visibility);*/
 /*
    * @brief 切换渲染模式
    * 
    * 将render中的装配体切换为指定模式下的装配体，同时清除合并操作中所遗留下来的actor
    * @param[in] renderMode 当前的渲染模式
    
    */
    /*Q_INVOKABLE void changeRenderer(QString renderMode);*/// MYVTKITEM_H
  // Q_INVOKABLE void blocksMerged(QString model_name, const std::vector<int>& block_ids, int father_block, const std::unordered_set<int>& father_block_patches);
   // /*
   //* @brief 更新组操作
   //*
   //* 调用ModelActor的update_group函数
   //* @param[in] model_name 模型名
   //* @param group_id 
   //* @param group_blocks 更新指定group的actor的mapper

   //*/
   // Q_INVOKABLE void groupUpdated(QString model_name, int group_id, const std::unordered_set<int>& group_blocks);
   // /*
   //* @brief 合并组操作
   //*
   //* 调用ModelActor的merge_groups函数
   //* @param[in] model_name 模型名
   //* @param group_ids 要合并的group
   //* @param father_group 留下的group
   //* @param father_group_blocks 留下的group合并后拥有的blocks id

   //*/
   // Q_INVOKABLE void groupMerged(QString model_name, const std::vector<int>& group_ids, int father_group, const std::unordered_set<int>& father_group_blocks);
   // /*
   //* @brief 更新片操作
   //*
   //* 调用ModelActor的update_patch函数
   //* @param[in] model_name 模型名
   //* @param patch_id 要更新的patch
   //* @param points 坐标数据
   //* @param triangles 三角形索引数组

   //*/
   // Q_INVOKABLE void patchUpdated(QString model_name, int patch_id, const std::vector<std::array<double, 3>>& points, const std::vector<std::array<int, 3>>& triangles);
   // /*
   //* @brief 更新组操作
   //*
   //* 调用ModelActor的update_block函数
   //* @param[in] model_name 模型名
   //* @param[in] block_id
   //* @param[in] block_patches 更新指定block的actor的mapper

   //*/
   // Q_INVOKABLE void blockUpdated(QString model_name, int block_id, const std::unordered_set<int>& block_patches);
   // /*
   // *@brief 删除模型
   // * 
   // * 渲染窗口中删除模型
   // * param[in] model_name 模型名
   // */