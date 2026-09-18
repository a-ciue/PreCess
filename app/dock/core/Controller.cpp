/**
 * @file Controller.cpp
 * @brief 控制器基类的实现
 */

#include "Controller.h"

namespace dock {

Controller::Controller(Type type, QObject* parent)
    : QObject(parent)
    , type_(type)
{
}

Controller::~Controller() = default;

}
