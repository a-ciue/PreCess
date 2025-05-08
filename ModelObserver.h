#pragma once
#include <QObject>
#include <QString>

/**
 * @brief QModelObserver：用于监听模型数据更改并向外通知的观察者类
 *
 * QModelObserver 通过槽函数接收模型层发出的事件，然后通过信号将这些事件通知给外部（例如 UI 层）。
 * 这样可以将模型数据的变化与界面解耦，当模型发生变化时，观察者发出相应的信号供 QML 或其他组件处理。
 */
class QModelObserver : public QObject {
Q_OBJECT
public:
    /**
     * @brief 构造 QModelObserver 对象
     * @param parent 父 QObject 指针，默认值为 nullptr
     */
    explicit QModelObserver(QObject* parent = nullptr) : QObject(parent) {}

public slots:
    /**
     * @brief 接收模型数据变更通知的槽函数
     * @param modelName 发生变化的模型名称
     *
     * 该槽函数由模型层在模型数据发生更改时调用，以便触发相应的信号通知。
     */
    void notifyModelChanged(const QString& modelName) {
        emit modelChanged(modelName);
    }

signals:
    /**
     * @brief 模型数据发生变化时发出的信号
     * @param modelName 发生变化的模型名称
     */
    void modelChanged(const QString& modelName);
};
