#ifndef TRIVIAL_EDIT_HANDLER_H
#define TRIVIAL_EDIT_HANDLER_H

#include "ComponentOperator.h"
#include "EditHandler.h"

#include <any>

namespace systems::edit {

class TrivialEditHandler : public EditHandler {
public:
    TrivialEditHandler() = default;
    ~TrivialEditHandler() override = default;

    std::any execute(ComponentOperator& /*op*/,
        const std::vector<core::ArgObject>& /*args*/) override
    {
        return {};
    }

    std::vector<core::ArgType> args_type() const override
    {
        return {};
    }
};

} // namespace systems::edit

#endif // TRIVIAL_EDIT_HANDLER_H