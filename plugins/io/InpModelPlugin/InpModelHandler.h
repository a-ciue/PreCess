#ifndef INP_MODEL_HANDLER_H
#define INP_MODEL_HANDLER_H
#include "ModelIOHandler.h"

#include "AbaqusPrecessConverter.h"
#include "abaqus_io.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace systems::io {
class InpModelHandler : public ModelIOHandler {
public:
    InpModelHandler() = default;
    ~InpModelHandler() override = default;

    std::optional<ModelPayload> read_model(const fs::path& path, const std::vector<std::any>& args) override;
    void write_components(const ModelLayer& mgr,
        const std::vector<Index>& component_ids,
        const fs::path& path,
        const std::vector<std::any>& args) override;
    std::vector<core::ArgType> read_args_type() const override;
    std::vector<core::ArgType> write_args_type() const override;
};

}
#endif // !INP_MODEL_HANDLER_H
