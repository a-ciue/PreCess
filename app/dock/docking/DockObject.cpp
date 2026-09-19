/**
 * @file DockObject.cpp
 * @brief 控制器基类的实现
 */

#include "DockObject.h"

namespace dock {

DockObject::DockObject(Kind kind, QObject* parent)
    : QObject(parent)
    , kind_(kind)
{
}

DockObject::~DockObject() = default;

}
