#pragma once
#include <dspsim/context.h>

namespace dspsim
{
    class Simulator
    {
    public:
        Simulator(ContextPtr context, double time_unit = 1e-9, double time_precision = 1e-9);
        ~Simulator();

        /*
            Properties
        */
        ContextPtr context() const { return _context; }

        double time_unit() const { return _time_unit; }
        void set_time_unit(double time_unit) { _time_unit = time_unit; }

        double time_precision() const { return _time_precision; }
        void set_time_precision(double time_precision) { _time_precision = time_precision; }

        void set_timescale(double time_unit, double time_precision)
        {
            set_time_unit(time_unit);
            set_time_precision(time_precision);
        }

        /*
            Methods
        */
        void eval();
        void run(uint64_t time_step);

    private:
        ContextPtr _context;
        double _time_unit;
        double _time_precision;
    };
}
