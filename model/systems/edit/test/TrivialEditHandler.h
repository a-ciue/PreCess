#ifndef TRIVIAL_EDIT_HANDLER_H
#define TRIVIAL_EDIT_HANDLER_H

#include "EditHandler.h"

#include <any>

namespace systems::edit {

class TrivialEditHandler : public EditHandler {
public:
    TrivialEditHandler() = default;
    ~TrivialEditHandler() override = default;

    std::any execute(ModelLayer& /*model*/, Index /*fallback_component_id*/,
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