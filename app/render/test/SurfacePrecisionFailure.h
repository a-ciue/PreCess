/**
 * @file SurfacePrecisionFailure.h
 * @brief 在真实重建完成处注入一次异常，不为生产代码增加测试开关。
 */
#pragma once

#include <Standard_Failure.hxx>
#include <spdlog/sinks/callback_sink.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

class SurfacePrecisionFailure {
public:
    enum class Kind { Standard,
        Occ,
        Unknown };

    explicit SurfacePrecisionFailure(int fail_on = 1, Kind kind = Kind::Standard)
        : original_logger_(spdlog::default_logger())
    {
        auto sink = std::make_shared<spdlog::sinks::callback_sink_mt>([this, fail_on](const spdlog::details::log_msg& message) {
            const std::string_view text(message.payload.data(), message.payload.size());
            if (text.starts_with("几何拓扑诊断"))
                ++diagnostic_logs_;
            if (text.starts_with("[SurfacePrecision]") && ++rebuilds_ == fail_on)
                throw std::runtime_error("injected rebuild failure");
        });
        auto logger = std::make_shared<spdlog::logger>("precision-failure", sink);
        logger->set_level(spdlog::level::info);
        // spdlog 默认会吞掉 sink 异常；显式让错误处理器将异常传回重建调用者。
        logger->set_error_handler([kind](const std::string&) {
            if (kind == Kind::Occ)
                throw Standard_Failure("injected surface precision failure");
            if (kind == Kind::Unknown)
                throw 42;
            throw std::runtime_error("injected surface precision failure");
        });
        spdlog::set_default_logger(logger);
    }

    ~SurfacePrecisionFailure() { spdlog::set_default_logger(original_logger_); }
    SurfacePrecisionFailure(const SurfacePrecisionFailure&) = delete;
    SurfacePrecisionFailure& operator=(const SurfacePrecisionFailure&) = delete;
    int rebuilds() const { return rebuilds_; }
    int diagnosticLogs() const { return diagnostic_logs_; }

private:
    std::shared_ptr<spdlog::logger> original_logger_;
    int rebuilds_ {};
    int diagnostic_logs_ {};
};
