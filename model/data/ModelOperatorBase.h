#ifndef MODEL_OPERATOR_BASE_H
#define MODEL_OPERATOR_BASE_H

#include "ModelData.h"
#include "Core.h"

class ModelOperatorBase {
public:
    virtual ~ModelOperatorBase() = default;
    virtual Index getId() const = 0;
};

#endif // MODEL_OPERATOR_BASE_H