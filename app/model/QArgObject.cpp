#include "QArgObject.h"
#include <QUrl>

#include "QSelection.h"

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

std::optional<std::any> QArgObject::getValue() const
{
    bool canConvert { true };
    std::any ret;
    switch (type_.type) {
    case ArgTypeEnum::None:
        break;
    case ArgTypeEnum::Int:
        ret = static_cast<long long>(value_.toLongLong(&canConvert));
        break;
    case ArgTypeEnum::Float:
        ret = value_.toDouble(&canConvert);
        break;
    case ArgTypeEnum::Text:
        ret = value_.toString().toStdString();
        break;
    case ArgTypeEnum::Bool:
        ret = value_.toBool();
        break;
    case ArgTypeEnum::Path:
        ret = QFileInfo(value_.toString()).filesystemFilePath();
        break;
    case ArgTypeEnum::Combo:
        // -1 意味着用户未进行选择
        ret = value_.toInt(&canConvert);
        break;
    case ArgTypeEnum::Selector: {
        QSelection* selection = value_.value<QSelection*>();
        canConvert = !!selection;
        if (selection) {
            ret = std::make_shared<std::unique_ptr<Selection>>(selection->move());
        }
        break;
    }
    default:
        canConvert = false;
        break;
    }

    return canConvert ? std::optional{ ret } : std::nullopt;
}