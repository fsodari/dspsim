#pragma once
#include <dspsim/vmodel.h>

// Include verilated model header
#include <VSimpleModel16.h>

// nanobind for binding function.
#include <nanobind/nanobind.h>

// Include appropriate trace header.
#include <verilated_fst_c.h>


class SimpleModel16 : public dspsim::VModel<VSimpleModel16, VerilatedFstC>
{
protected:
    // Ports
    dspsim::InputPtr<uint8_t> _clk;
    dspsim::InputPtr<uint8_t> _rst;
    dspsim::InputPtr<uint16_t> _i;
    dspsim::OutputPtr<uint16_t> _o;

public:
    // Parameters
    SimpleModel16(
        dspsim::SignalPtr<uint8_t> clk,
        dspsim::SignalPtr<uint8_t> rst,
        dspsim::SignalPtr<uint16_t> i,
        dspsim::SignalPtr<uint16_t> o,
        const std::string &name = "")
        : VModel<VSimpleModel16, VerilatedFstC>(name)
    {
        _clk = dspsim::Model::create<dspsim::Input<uint8_t>>(clk, top->clk, id(), "clk");
        _rst = dspsim::Model::create<dspsim::Input<uint8_t>>(rst, top->rst, id(), "rst");
        _i = dspsim::Model::create<dspsim::Input<uint16_t>>(i, top->i, id(), "i");
        _o = dspsim::Model::create<dspsim::Output<uint16_t>>(o, top->o, id(), "o");

        this->_kind = "SimpleModel16";
    }

    static auto create(
        dspsim::SignalPtr<uint8_t> clk,
        dspsim::SignalPtr<uint8_t> rst,
        dspsim::SignalPtr<uint16_t> i,
        dspsim::SignalPtr<uint16_t> o,
        const std::string &name = "")
    {
        return dspsim::Model::create<SimpleModel16>(
            clk,
            rst,
            i,
            o,
            name);
    }
};

static auto bind_SimpleModel16(nanobind::module_ &_m)
{
    return nanobind::class_<SimpleModel16, dspsim::Model>(_m, "SimpleModel16")
        .def(nanobind::new_(&SimpleModel16::create),
            nanobind::arg("clk"),
            nanobind::arg("rst"),
            nanobind::arg("i"),
            nanobind::arg("o"),
             nanobind::kw_only(),
             nanobind::arg("name") = "")
        // .def_prop_rw("name", &SimpleModel16::name, &SimpleModel16::set_name)
        .def("trace", &SimpleModel16::trace,
             nanobind::arg("trace_path"),
             nanobind::arg("levels") = 99,
             nanobind::arg("options") = 0)
        .def("close", &SimpleModel16::close);
}