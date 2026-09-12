#include <dspsim/simulator.h>
#include <dspsim/model.h>
#include "timestrings.h"

namespace dspsim
{
    Simulator::Simulator(ContextPtr context, const std::string &time_unit, const std::string &time_precision) : _context(context)
    {
        _context->set_timescale(time_unit, time_precision);
        start();
    }

    Simulator::~Simulator()
    {
    }

    void Simulator::start()
    {
        _sim_models.clear();
        for (const auto &m : context()->models())
        {
            // Exclude output ports from the simulation models list.
            // Output ports do not need to be evaluated.
            // Alternatively, output models could be simulated last.
            if (m->kind() != "output")
            {
                _sim_models.push_back(m.get());
            }
        }

        // Evaluate output ports last.
        for (const auto &m : context()->models())
        {
            if (m->kind() == "output")
            {
                _sim_models.push_back(m.get());
            }
        }

        _time_step = calc_time_step(context()->time_unit(), context()->time_precision());
    }

    void Simulator::eval()
    {
        for (const auto &m : _sim_models)
        {
            m->eval_step();
        }
        for (const auto &m : _sim_models)
        {
            m->eval_end_step();
        }
    }

    void Simulator::run(uint64_t time_inc)
    {
        uint64_t n_steps = static_cast<uint64_t>(time_inc * _time_step);
        for (uint64_t i = 0; i < n_steps; ++i)
        {
            eval();
            context()->_time++;
        }
    }
}