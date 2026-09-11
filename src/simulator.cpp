#include <dspsim/simulator.h>
#include <dspsim/model.h>

namespace dspsim
{
    Simulator::Simulator(ContextPtr context, double time_unit, double time_precision) : _context(context)
    {
        set_timescale(time_unit, time_precision);
    }

    Simulator::~Simulator()
    {
    }

    void Simulator::eval()
    {
        for (const auto &m : _context->models())
        {
            m->eval_step();
        }
        for (const auto &m : _context->models())
        {
            m->eval_end_step();
        }
    }

    void Simulator::run(uint64_t time_step)
    {
        uint64_t n_steps = static_cast<uint64_t>(time_step * _time_precision);
        for (uint64_t i = 0; i < n_steps; ++i)
        {
            eval();
            _context->_time += _time_precision;
        }
    }
}