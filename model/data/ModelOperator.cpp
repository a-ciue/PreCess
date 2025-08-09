#ifndef Model_OPERATOR_H
#define Model_OPERATOR_H
#include "ModelOperator.h"
//#include "../FileHandler.h"
#include "ModelObserver.h"

ModelData* ModelOperator::data() const
{
    return model_;
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
    observer_->notifyModelChanged(model_->getId());
}

void ModelOperator::merge_blocks(std::unique_ptr<Selection> selection)
{
    if (!selection) {
        return; // 如果没有选择或选择为空，直接返回
    }
    model_->merge_blocks(*selection);
    observer_->notifyModelChanged(model_->getId());
}

Index ModelOperator::getId() const
{
    return model_->id_;
}

ModelData::Type ModelOperator::getType() const
{
    return model_->type();
}

#endif // Model_OPERATOR_H
