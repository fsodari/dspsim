#pragma once
#include <dspsim/context.h>

namespace dspsim
{
    class Simulator
    {
    public:
        Simulator(ContextPtr context, const std::string &time_unit = "1ns", const std::string &time_precision = "1ns");
        ~Simulator();

        /*
            Properties
        */
        ContextPtr context() const { return _context; }

        void start();
        void eval();
        void run(uint64_t time_step);

    private:
        ContextPtr _context;

        std::vector<Model *> _sim_models;

        uint64_t _time_step;
    };
}
