#pragma once
#include <dspsim/vmodel.h>

// Include verilated model header
#include <VSimpleModel8.h>

// nanobind for binding function.
#include <nanobind/nanobind.h>

// Include appropriate trace header.
#include <verilated_vcd_c.h>


class SimpleModel8 : public dspsim::VModel<VSimpleModel8>
{
protected:
    // Ports
    dspsim::InputPtr<uint8_t> _clk;
    dspsim::InputPtr<uint8_t> _rst;
    dspsim::InputPtr<uint8_t> _i;
    dspsim::OutputPtr<uint8_t> _o;

public:
    // Parameters
    inline static const int DW = 8;
    SimpleModel8(
        dspsim::SignalPtr<uint8_t> clk,
        dspsim::SignalPtr<uint8_t> rst,
        dspsim::SignalPtr<uint8_t> i,
        dspsim::SignalPtr<uint8_t> o,
        const std::string &name = "")
        : VModel<VSimpleModel8>(name)
    {
        _clk = dspsim::Model::create<dspsim::Input<uint8_t>>(clk, top->clk, id(), "clk");
        _rst = dspsim::Model::create<dspsim::Input<uint8_t>>(rst, top->rst, id(), "rst");
        _i = dspsim::Model::create<dspsim::Input<uint8_t>>(i, top->i, id(), "i");
        _o = dspsim::Model::create<dspsim::Output<uint8_t>>(o, top->o, id(), "o");

        this->_kind = "SimpleModel8";
    }

    static auto create(
        dspsim::SignalPtr<uint8_t> clk,
        dspsim::SignalPtr<uint8_t> rst,
        dspsim::SignalPtr<uint8_t> i,
        dspsim::SignalPtr<uint8_t> o,
        const std::string &name = "")
    {
        return dspsim::Model::create<SimpleModel8>(
            clk,
            rst,
            i,
            o,
            name);
    }

    static inline auto bind_SimpleModel8(nanobind::module_ &_m)
    {
        return nanobind::class_<SimpleModel8, dspsim::Model>(_m, "SimpleModel8")
            .def(nanobind::new_(&SimpleModel8::create),
                nanobind::arg("clk"),
                nanobind::arg("rst"),
                nanobind::arg("i"),
                nanobind::arg("o"),
                nanobind::kw_only(),
                nanobind::arg("name") = "")
            // .def_prop_rw("name", &SimpleModel8::name, &SimpleModel8::set_name)
            .def_ro_static("DW", &SimpleModel8::DW)
            .def("trace", &SimpleModel8::trace,
                nanobind::arg("trace_path"),
                nanobind::arg("levels") = 99,
                nanobind::arg("options") = 0)
            .def("close", &SimpleModel8::close);
    }
};

