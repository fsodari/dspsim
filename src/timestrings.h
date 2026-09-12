#pragma once
#include <string>
#include <cstdint>

/*
    Converts a time string with magnitude and unit to a double representing seconds.
    Supported formats: "1fs", "10fs", "100fs", "1ps", "10ps", "100ps", "1ns", "10ns", "100ns",
                       "1us", "10us", "100us", "1ms", "10ms", "100ms", "ms", "1s", "10s", "100s"

    Using a table instead of parsing because C++ isn't as easy as python.
*/
constexpr double time_string_to_double(std::string_view ts)
{
    if (ts == "fs")
        return 1e-15;
    if (ts == "1fs")
        return 1e-15;
    if (ts == "10fs")
        return 10e-15;
    if (ts == "100fs")
        return 100e-15;
    if (ts == "ps")
        return 1e-12;
    if (ts == "1ps")
        return 1e-12;
    if (ts == "10ps")
        return 10e-12;
    if (ts == "100ps")
        return 100e-12;
    if (ts == "ns")
        return 1e-9;
    if (ts == "1ns")
        return 1e-9;
    if (ts == "10ns")
        return 10e-9;
    if (ts == "100ns")
        return 100e-9;
    if (ts == "us")
        return 1e-6;
    if (ts == "1us")
        return 1e-6;
    if (ts == "10us")
        return 10e-6;
    if (ts == "100us")
        return 100e-6;
    if (ts == "ms")
        return 1e-3;
    if (ts == "1ms")
        return 1e-3;
    if (ts == "10ms")
        return 10e-3;
    if (ts == "100ms")
        return 100e-3;
    if (ts == "ms")
        return 1e-3;
    if (ts == "s")
        return 1.0;
    if (ts == "1s")
        return 1.0;
    if (ts == "10s")
        return 10.0;
    if (ts == "100s")
        return 100.0;

    return 0.0; // Default case, could throw an exception instead
}

uint64_t calc_time_step(std::string_view time_unit, std::string_view time_precision)
{
    return time_string_to_double(time_unit) / time_string_to_double(time_precision);
}