/**
 * @file SurfacePrecisionFailure.h
 * @brief 在真实重建完成处注入一次异常，不为生产代码增加测试开关。
 */
#pragma once

#include <Standard_Failure.hxx>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <mutex>
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
        , sink_(std::make_shared<FailureSink>(fail_on))
    {
        auto logger = std::make_shared<spdlog::logger>("precision-failure", sink_);
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
    int rebuilds() const { return sink_->rebuilds; }

private:
    struct FailureSink : spdlog::sinks::base_sink<std::mutex> {
        explicit FailureSink(int fail_on)
            : fail_on(fail_on)
        {
        }
        void sink_it_(const spdlog::details::log_msg& message) override
        {
            const std::string_view text(message.payload.data(), message.payload.size());
            if (text.starts_with("[SurfacePrecision]") && ++rebuilds == fail_on)
                throw std::runtime_error("injected rebuild failure");
        }
        void flush_() override { }
        int fail_on;
        int rebuilds {};
    };
    std::shared_ptr<spdlog::logger> original_logger_;
    std::shared_ptr<FailureSink> sink_;
};
