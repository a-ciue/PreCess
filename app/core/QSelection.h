#ifndef  Q_SELECTION_H
#define Q_SELECTION_H 
#include <QObject>
#include <qqmlintegration.h>
#include "Selection.h"

//! @brief Element在QML中暴露了枚举Element::Type，在QML中以Element.Face获取枚举值
class Element : public QObject, public ElementEnum {
    Q_OBJECT
    QML_ELEMENT
public:
    Q_ENUM(Type)
};

/**
 * @brief Selection类的QML交互接口。Selection可以存于QSelection中，就可以在QML代码中进行交互。C++中使用new来创建对象，QML中为只读对象
 */
class QSelection : public QObject {
    Q_OBJECT
    QML_ELEMENT

public:
    QSelection() { }
    QSelection(std::unique_ptr<Selection> data)
        : data_(std::move(data))
    {
    }

    /**
     * 存入
     * @param data 待存入的数据
     */
    void set(std::shared_ptr<Selection> data) { data_ = std::move(data); }
    /**
     * 取出
     * @return 存储的Selection对象
     */
    std::shared_ptr<Selection> get() { return data_; }

    /**
     * @brief 获取该选择的数组大小
     * @return 存储数组大小
     */
    Q_INVOKABLE size_t size()
    {
        if (data_) {
            return data_->ids.size();
        }
        return 0;
    }
    /**
     * @brief 获取选中数据的类型枚举值
     * @return 类型枚举值
     */
    Q_INVOKABLE Element::Type type() { return data_->type; }
    /**
     * @brief 初始化一个Selection数据
     */
    Q_INVOKABLE void initialize()
    {
        std::unique_ptr<Selection> temp = std::make_unique<Selection>();
        temp->type = ElementEnum::Face;
        for (int i = 1; i < 2; i++) {
            temp->ids.push_back(i);
        }
        data_ = std::move(temp);
    }
    /**
     * @brief ids 返回选择对象的id值表的第i个元素
     * @param i   想要返回第几个元素
     * @return    int类
     */
    Q_INVOKABLE int ids(int i)
    {
        return data_->ids[i];
    }
    /**
     * @brief 返回选中对象的模型 ID
     * @return 模型id
     */
    Q_INVOKABLE Index getModelId()
    {
        return data_->model_id;
    }

private:
    std::shared_ptr<Selection> data_;
};

#endif //  Q_SELECTION_H
