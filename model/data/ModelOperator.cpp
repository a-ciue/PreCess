#ifndef Model_OPERATOR_H
#define Model_OPERATOR_H
#include "ModelOperator.h"
//#include "../FileHandler.h"
#include "ModelObserver.h"

ModelData& ModelOperator::data() const
{
    return *model_;
}

ModelObserver* ModelOperator::observer() const
{
    return observer_;
}

void ModelOperator::write_spline(const std::filesystem::path& spline_path)
{
}

void ModelOperator::notifyChanged()
{
    if (this->observer_) {
        observer_->notifyModelChanged(this->id_);
    }
}

Index ModelOperator::getId() const
{
    return this->id_;
}

#endif // Model_OPERATOR_H
