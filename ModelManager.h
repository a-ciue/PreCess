/**
 * @file ModelManager.h
 * @brief 负责管理多个模型实例的类
 *
 * ModelManager 仅负责管理模型（添加、删除、查询、重命名）以及与 VTK 组件的交互。
 * 与文件 IO、样条转换相关的功能已全部移至 FileHandler。
 *
 * @author 徐昊阳 haoyangxu06@gmail.com
 * @date 2025/3/20
 */

#ifndef MODELMANAGER_H
#define MODELMANAGER_H
#include "Model.h"
#include "MyVtkItem.h"

#include <qqmlregistration.h>
#include <QQmlContext>
#include <QObject>

 /**
  * @brief 负责管理多个 Model 实例的类
  *
  * ModelManager 允许动态添加、删除和查找模型，并提供与 VTK 组件的交互接口，
  * 使得 QML 层能够访问和控制网格数据。
  */
class ModelManager : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QRenderWindow* vtkItem READ vtkItem WRITE setVtkItem)
    

public:
    /**
     * @brief 构造 ModelManager 对象
     *
     * @param parent 父对象，默认为 nullptr
     */
    explicit ModelManager(QObject* parent = nullptr) : QObject(parent) {}

    /**
     * @brief 获取指定名称的模型
     *
     * 在管理的模型中查找对应名称的 Model 实例。
     *
     * @param modelName 要查找的模型名称
     * @return Model* 若找到模型，则返回指针；否则返回 nullptr
     */
    Q_INVOKABLE Model* model(const QString& modelName) {
        auto it = models_.find(modelName);
        if (it != models_.end()) {
            return it->second.get();
        }
        return nullptr; // 如果找不到模型，返回空指针
    }

    /**
     * @brief 获取当前 VTK 组件
     *
     * @return MyVtkItem* 指向当前 VTK 组件的指针
     */
    QRenderWindow* vtkItem();

    /**
     * @brief 设置 VTK 组件
     *
     * @param item 需要关联的 VTK 组件
     */
    void setVtkItem(QRenderWindow* item);

    /**
     * @brief 添加一个模型
     *
     * @param modelName 新模型的名称
     * @param model 需要添加的模型对象
     */
    void addModel(const QString& modelName, std::unique_ptr<Model> model);
    
    /**
     * @brief 移除指定名称的模型
     *
     * @param modelName 要移除的模型名称
     */
    Q_INVOKABLE void removeModel(const QString& modelName);

    /**
     * @brief 获取指定名称的模型
     *
     * @param modelName 要查找的模型名称
     * @return Model* 若找到模型，则返回指针；否则返回 nullptr
     */
    Q_INVOKABLE Model* getModel(const QString& modelName) const;

    /**
     * @brief 读取样条曲线数据
     *
     * @param spline_path 样条曲线文件的路径
     */
    Q_INVOKABLE void readSpline(QUrl spline_path);

    /**
     * @brief 读取网格数据
     *
     * @param target_mesh 网格文件的路径
     */
    Q_INVOKABLE void readMesh(QUrl target_mesh);

    /**
     * @brief 输出网格数据
     *
     * @param modelName 需要导出的模型名称
     * @param target_mesh 输出文件的路径
     * @param renderMode 选择的渲染模式
     * @param extension 输出文件的扩展名
     */
    Q_INVOKABLE void writeMesh(const QString& modelName, QUrl target_mesh, QString renderMode, QString extension);

    /**
     * @brief 重命名模型
     *
     * @param oldName 旧名称
     * @param newName 新名称
     */
    Q_INVOKABLE void renameModel(const QString& oldName, const QString& newName);


signals:
    /**
     * @brief 样条曲线加载失败信号
     *
     * @param message 失败信息
     */
    void splineLoadFailed(QString message);

    /**
     * @brief 模型名称变更信号
     *
     * @param oldName 旧模型名称
     * @param newName 新模型名称
     */
    void modelNameChanged(const QString& oldName, const QString& newName);
    
    /**
     * @brief 新模型添加信号
     *
     * @param modelName 添加的模型名称
     */
    void modelAdded(const QString& modelName);
    
    /**
     * @brief 模型移除信号
     *
     * @param modelName 移除的模型名称
     */
    void modelRemoved(const QString& modelName);


private:
    //std::unique_ptr<Model> model_;
    //使用unordered_map替代原unique_ptr用于满足存储多模型的要求
    std::unordered_map<QString, std::unique_ptr<Model>> models_; 
    QRenderWindow* vtk_item_{};
};
#endif // MODELMANAGER_H
