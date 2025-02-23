#ifndef SELECTION_H
#define SELECTION_H
#include <QObject>
#include <qqmlintegration.h>
#include <vector>

//! @brief Element在QML中暴露了枚举Element::Type，在QML中以Element.Face获取枚举值
class Element : public QObject {
    Q_OBJECT
        QML_ELEMENT
public:
    enum Type {
        Face,
        Edge,
        Vertex,
        Block,
        Group

    };
    Q_ENUM(Type)
};

//! @brief 存储选择的对象
struct Selection {
    //! @brief 选择对象的id序列
    std::vector<int> ids;
    //! @brief 选择对象的类型
    Element::Type type;
    QString model_name;
};

/**
 * @brief Selection类的QML交互接口。Selection可以存于QSelection中，就可以在QML代码中进行交互。C++中使用new来创建对象，QML中为只读对象
 */
class QSelection : public QObject
{
    Q_OBJECT
        QML_ELEMENT

public:
    QSelection() {}
    QSelection(std::unique_ptr<Selection> data)
        : _data(std::move(data)) {
    }
    ~QSelection() {
        std::cout << "okokok" << endl;
    }

    /**
     * 存入
     * @param data 待存入的数据
     */
    void set(std::unique_ptr<Selection> data) { _data = std::move(data); }
    /**
     * 取出
     * @return 存储的Selection对象
     */
    std::unique_ptr<Selection> move() { return std::move(_data); }

    /**
     * @brief 获取该选择的数组大小
     * @return 存储数组大小
     */
    Q_INVOKABLE size_t size() {
        if (_data) {
            return _data->ids.size();
        }
        return 0;
    }
    /**
     * @brief 获取选中数据的类型枚举值
     * @return 类型枚举值
     */
    Q_INVOKABLE Element::Type type() { return _data->type; }
    /**
     * @brief 初始化一个Selection数据
     */
    Q_INVOKABLE void initialize() {
        std::unique_ptr<Selection> temp = std::make_unique<Selection>();
        temp->type = Element::Face;
        for (int i = 0; i < 10; i++) {
            temp->ids.push_back(i);
        }
        _data = std::move(temp);
    }
    /**
     * @brief ids 返回选择对象的id值表的第i个元素
     * @param i   想要返回第几个元素
     * @return    int类
     */
    Q_INVOKABLE int ids(int i) {
        return _data->ids[i];
    }

private:
    std::unique_ptr<Selection> _data;
};

#endif // SELECTION_H