#include "QArgObject.h"
#include "ArgObject.h"
#include "QSelection.h"
#include <QFileInfo>

using namespace core;
QArgObject::QArgObject(ArgType&& type, QObject* parent)
    : QObject(parent)
    , type_(std::move(type))
{
}

QArgObject::QArgObject(const ArgType& type, QObject* parent)
    : QObject(parent)
    , type_(type)
{
}

QArgType::ArgTypeEnum QArgObject::type() const { return static_cast<QArgType::ArgTypeEnum>(type_.type); }

QString QArgObject::name() const { return QString::fromStdString(type_.name); }

QString QArgObject::content() const { return QString::fromStdString(type_.content); }

QString QArgObject::desc() const
{
    return QString::fromStdString(type_.desc);
}

std::optional<ArgObject> QArgObject::getValue() const
{
    bool canConvert { true };
    std::optional<ArgObject> ret;
    switch (type_.type) {
    case ArgTypeEnum::None:
        break;
    case ArgTypeEnum::Int:
        ret = ArgObject::create<ArgTypeEnum::Int>(static_cast<long long>(value_.toLongLong(&canConvert)));
        break;
    case ArgTypeEnum::Float:
        ret = ArgObject::create<ArgTypeEnum::Float>(value_.toDouble(&canConvert));
        break;
    case ArgTypeEnum::Text:
        ret = ArgObject::create<ArgTypeEnum::Text>(value_.toString().toStdString());
        break;
    case ArgTypeEnum::Bool:
        ret = ArgObject::create<ArgTypeEnum::Bool>(value_.toBool());
        break;
    case ArgTypeEnum::Path:
        ret = ArgObject::create<ArgTypeEnum::Path>(QFileInfo(value_.toString()).filesystemFilePath());
        break;
    case ArgTypeEnum::Combo:
        // -1 意味着用户未进行选择
        ret = ArgObject::create<ArgTypeEnum::Combo>(value_.toInt(&canConvert));
        break;
    case ArgTypeEnum::Selector: {
        QSelection* selection = value_.value<QSelection*>();
        if (selection) {
            canConvert = true;
            ret = ArgObject::create<ArgTypeEnum::Selector>(std::make_shared<std::unique_ptr<Selection>>(selection->move()));
        }
        break;
    }
    default:
        canConvert = false;
        break;
    }

    return canConvert ? ret : std::nullopt;
}

void QArgObject::setValue(const QVariant& value)
{
    value_ = value;
    emit valueChanged();
}