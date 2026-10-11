/*
    Logging front end of the engine. Internal to dspsim-core: not installed.

    spdlog is used header-only, and every source file that includes it spends several seconds compiling
    it. Only log.cpp includes spdlog. The rest of the core logs through Logger, which takes messages that
    are already formatted, using the DSPSIM_LOG_* macros below. The macros evaluate the message expression
    only if the level is enabled.
*/
#pragma once
#include <memory>
#include <string>
#include <string_view>

namespace spdlog
{
    class logger;
}

namespace dspsim::detail
{
    // Same values as spdlog::level::level_enum.
    enum class LogLevel
    {
        Trace,
        Debug,
        Info,
        Warn,
        Error,
        Critical,
        Off
    };

    class Logger
    {
    public:
        /// Create a logger that writes to stdout at level "warn". The name must be unique among all loggers.
        explicit Logger(const std::string &name);
        ~Logger();

        /// True if a message at this level would be written.
        bool enabled(LogLevel level) const;
        void log(LogLevel level, std::string_view message);
        /// Log at a level given by name. See set_level().
        void log(const std::string &level, std::string_view message);

        /// Name of the current level: "trace", "debug", "info", "warning", "error", "critical" or "off".
        std::string level() const;
        /// Set the level by name (case insensitive). An unknown name selects "info".
        void set_level(const std::string &level);

    private:
        std::shared_ptr<spdlog::logger> logger_;
    };
} // namespace dspsim::detail

// Compile-time log level: messages below it are compiled out. 0 = trace, 1 = debug, 2 = info (the default).
// Set per build type in src/CMakeLists.txt.
#ifndef DSPSIM_LOG_ACTIVE_LEVEL
#define DSPSIM_LOG_ACTIVE_LEVEL 2
#endif

// Log a message (a std::string or string literal expression) to a Logger pointer if the level is enabled.
#define DSPSIM_LOG_AT(logger, log_level, message)                  \
    do                                                             \
    {                                                              \
        if ((logger)->enabled(::dspsim::detail::LogLevel::log_level)) \
        {                                                          \
            (logger)->log(::dspsim::detail::LogLevel::log_level, message); \
        }                                                          \
    } while (false)

#if DSPSIM_LOG_ACTIVE_LEVEL <= 0
#define DSPSIM_LOG_TRACE(logger, message) DSPSIM_LOG_AT(logger, Trace, message)
#else
#define DSPSIM_LOG_TRACE(logger, message) (void)0
#endif

#if DSPSIM_LOG_ACTIVE_LEVEL <= 1
#define DSPSIM_LOG_DEBUG(logger, message) DSPSIM_LOG_AT(logger, Debug, message)
#else
#define DSPSIM_LOG_DEBUG(logger, message) (void)0
#endif

#define DSPSIM_LOG_INFO(logger, message) DSPSIM_LOG_AT(logger, Info, message)
#define DSPSIM_LOG_WARN(logger, message) DSPSIM_LOG_AT(logger, Warn, message)
#define DSPSIM_LOG_ERROR(logger, message) DSPSIM_LOG_AT(logger, Error, message)
