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
        ret = static_cast<size_t>(value_.toULongLong(&canConvert));
        break;
    case ArgTypeEnum::Text:
        ret = value_.toString().toStdString();
        break;
    case ArgTypeEnum::Bool:
        ret = value_.toBool();
        break;
    case ArgTypeEnum::Path:
        ret = QFileInfo(value_.toUrl().toLocalFile()).filesystemPath();
        break;
    case ArgTypeEnum::Combo:
		// TODO: 填充Combo逻辑
        break;
    case ArgTypeEnum::Selector:
        ret = std::make_shared<std::unique_ptr<Selection>>(value_.value<QSelection*>()->move());
        break;
    }

    return std::nullopt;
}