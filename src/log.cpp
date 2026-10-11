#include "log.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <algorithm>
#include <cctype>

namespace dspsim::detail
{
    namespace
    {
        spdlog::level::level_enum to_spdlog(LogLevel level)
        {
            return static_cast<spdlog::level::level_enum>(static_cast<int>(level));
        }

        spdlog::level::level_enum parse_level(const std::string &level)
        {
            // Convert to lowercase
            std::string lower_log_level = level;
            std::transform(lower_log_level.begin(), lower_log_level.end(), lower_log_level.begin(), ::tolower);
            spdlog::level::level_enum parsed_level = spdlog::level::from_str(lower_log_level);
            if (parsed_level == spdlog::level::off && lower_log_level != "off")
            {
                // Invalid string level, use info as fallback.
                return spdlog::level::info;
            }
            else
            {
                return parsed_level;
            }
        }
    } // namespace

    Logger::Logger(const std::string &name)
        : logger_(spdlog::stdout_color_mt(name))
    {
        logger_->set_level(spdlog::level::warn);
        logger_->set_pattern("[%^%l%$] %v");
    }

    // Do not touch spdlog's global registry here: a Context can be destroyed by the static destruction of
    // the global context factory, and spdlog's registry (a function-local static) may already have been
    // torn down by that point, making spdlog::drop() dereference freed memory.
    Logger::~Logger() = default;

    bool Logger::enabled(LogLevel level) const
    {
        return logger_->should_log(to_spdlog(level));
    }

    void Logger::log(LogLevel level, std::string_view message)
    {
        logger_->log(to_spdlog(level), message);
    }

    void Logger::log(const std::string &level, std::string_view message)
    {
        logger_->log(parse_level(level), message);
    }

    std::string Logger::level() const
    {
        auto view = spdlog::level::to_string_view(logger_->level());
        return std::string{view.begin(), view.end()};
    }

    void Logger::set_level(const std::string &level)
    {
        logger_->set_level(parse_level(level));
    }
} // namespace dspsim::detail
