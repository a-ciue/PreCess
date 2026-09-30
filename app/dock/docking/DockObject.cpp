/**
 * @file DockObject.cpp
 * @brief 控制器基类的实现
 */

#include "DockObject.h"

namespace dock {

DockObject::DockObject(QObject* parent)
    : QObject(parent)
{
}

DockObject::~DockObject() = default;

}
