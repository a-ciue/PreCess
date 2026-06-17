#ifndef MODEL_PAYLOAD_H
#define MODEL_PAYLOAD_H

#include "ComponentData.h"
#include <string>

struct ModelPayload {
    std::string model_name;
    ComponentDatas components;
};

#endif // MODEL_PAYLOAD_H
