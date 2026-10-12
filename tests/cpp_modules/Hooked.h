#pragma once
#include <dspsim/module.h>
#include <dspsim/port.h>

#include <cstdint>

namespace demo
{
    /// Types that the builder knows nothing about: hooked_bindings.h binds them.
    struct Config
    {
        int gain = 1;
        int offset = 0;
    };

    enum class Mode
    {
        Bypass,
        Scale,
    };

    struct Hooked : public dspsim::Module
    {
        dspsim::Input<uint8_t> clk{"clk"};

        Hooked(dspsim::ModuleName)
        {
        }
        Hooked(dspsim::ModuleName, const Config &config) : config_(config)
        {
        }

        const Config &config() const { return config_; }
        void configure(const Config &config) { config_ = config; }
        Mode mode() const { return mode_; }
        void set_mode(Mode mode) { mode_ = mode; }
        int apply(int value) const { return mode_ == Mode::Scale ? value * config_.gain + config_.offset : value; }

        /// A signature that the builder cannot bind. hooked_bindings.h binds it in another form.
        void fill(int *values, int n) const
        {
            for (int i = 0; i < n; ++i)
            {
                values[i] = apply(i);
            }
        }

    private:
        Config config_;
        Mode mode_ = Mode::Scale;
    };
} // namespace demo
