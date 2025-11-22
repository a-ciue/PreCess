#ifndef TRIVIAL_EDIT_HANDLER_H
#define TRIVIAL_EDIT_HANDLER_H
#include "EditHandler.h"

namespace systems::edit {
class TrivialEditHandler : public EditHandler {
public:
    TrivialEditHandler() = default;
    ~TrivialEditHandler() override = default;
    ModelData execute(ModelData model_data, const std::vector<core::ArgObject>& args) override
    {
        return std::move(model_data);
    }
    std::vector<core::ArgType> args_type() const override
    {
        return {};
    }
};
}

#endif
