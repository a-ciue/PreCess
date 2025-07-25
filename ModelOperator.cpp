#ifndef Model_OPERATOR_H
#define Model_OPERATOR_H
#include "ModelOperator.h"
#include "FileHandler.h"
#include "ModelObserver.h"
#include "QSelection.h"

ModelData* ModelOperator::data() const
{
    return model_;
}

QModelObserver* ModelOperator::observer() const
{
    return observer_;
}

void ModelOperator::write_spline(const std::filesystem::path& spline_path)
{
    if (SplineData* spline = model_->asSplineData()) {
        FileHandler::instance().writeSpline(*spline, spline_path);
    }
}

void ModelOperator::notifyChanged()
{
    observer_->notifyModelChanged(model_->getId());
}

void ModelOperator::merge_blocks(QSelection* selection)
{
    model_->merge_blocks(*selection->move());
    observer_->notifyModelChanged(model_->getId());
}

int ModelOperator::getId() const
{
    return model_->id_;
}

#endif // Model_OPERATOR_H
