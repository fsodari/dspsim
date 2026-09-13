#pragma once
#include <dspsim/vmodel.h>

// Include verilated model header
#include <VSimpleModel.h>

// nanobind for binding function.
#include <nanobind/nanobind.h>

// Include appropriate trace header.
#include <verilated_vcd_c.h>


class SimpleModel : public dspsim::VModel<VSimpleModel, VerilatedVcdC>
{
protected:
    // Ports
    dspsim::InputPtr<uint8_t> _clk;
    dspsim::InputPtr<uint8_t> _rst;
    dspsim::InputPtr<uint32_t> _i;
    dspsim::OutputPtr<uint32_t> _o;

public:
    // Parameters
    static const int DW = 24;
    SimpleModel(
        dspsim::SignalPtr<uint8_t> clk,
        dspsim::SignalPtr<uint8_t> rst,
        dspsim::SignalPtr<uint32_t> i,
        dspsim::SignalPtr<uint32_t> o,
        const std::string &name = "")
        : VModel<VSimpleModel, VerilatedVcdC>(name)
    {
        _clk = dspsim::Model::create<dspsim::Input<uint8_t>>(clk, top->clk, id(), "clk");
        _rst = dspsim::Model::create<dspsim::Input<uint8_t>>(rst, top->rst, id(), "rst");
        _i = dspsim::Model::create<dspsim::Input<uint32_t>>(i, top->i, id(), "i");
        _o = dspsim::Model::create<dspsim::Output<uint32_t>>(o, top->o, id(), "o");

        this->_kind = "SimpleModel";
    }

    static auto create(
        dspsim::SignalPtr<uint8_t> clk,
        dspsim::SignalPtr<uint8_t> rst,
        dspsim::SignalPtr<uint32_t> i,
        dspsim::SignalPtr<uint32_t> o,
        const std::string &name = "")
    {
        return dspsim::Model::create<SimpleModel>(
            clk,
            rst,
            i,
            o,
            name);
    }
};

static auto bind_SimpleModel(nanobind::module_ &_m)
{
    return nanobind::class_<SimpleModel, dspsim::Model>(_m, "SimpleModel")
        .def(nanobind::new_(&SimpleModel::create),
            nanobind::arg("clk"),
            nanobind::arg("rst"),
            nanobind::arg("i"),
            nanobind::arg("o"),
             nanobind::kw_only(),
             nanobind::arg("name") = "")
        // .def_prop_rw("name", &SimpleModel::name, &SimpleModel::set_name)
        .def_ro_static("DW", &SimpleModel::DW)
        .def("trace", &SimpleModel::trace,
             nanobind::arg("trace_path"),
             nanobind::arg("levels") = 99,
             nanobind::arg("options") = 0)
        .def("close", &SimpleModel::close);
}