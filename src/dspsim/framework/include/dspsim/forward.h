/*
    Forward declarations of dspsim types.
*/
#pragma once
#include <memory>
namespace dspsim
{
    // class Context;
    class Model;
    class ModuleName;
    class Module;
    class PortBase;
    class InputBase;
    class OutputBase;
    template <typename T>
    class Input;
    template <typename T>
    class Output;
    class SignalBase;
    template <typename T>
    class Signal;
    class Clock;

    using ModelPtr = std::shared_ptr<Model>;
    using ModulePtr = std::shared_ptr<Module>;
}