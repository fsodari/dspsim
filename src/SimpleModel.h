#pragma once
#include <dspsim/vmodel.h>
#include <VSimpleModel.h>
// #include <verilated_vcd_c.h>
#include <verilated_fst_c.h>

#include <nanobind/nanobind.h>

class SimpleModel : public dspsim::VModel<VSimpleModel, VerilatedFstC>
{
protected:
    // Ports
    dspsim::InputPtr<uint8_t> _clk;
    dspsim::InputPtr<uint8_t> _rst;
    dspsim::InputPtr<uint8_t> _i;
    dspsim::OutputPtr<uint8_t> _o;

public:
    // Parameters
    SimpleModel(
        dspsim::SignalPtr<uint8_t> clk,
        dspsim::SignalPtr<uint8_t> rst,
        dspsim::SignalPtr<uint8_t> i,
        dspsim::SignalPtr<uint8_t> o,
        const std::string &name = "")
        : VModel<VSimpleModel, VerilatedFstC>(name)
    {
        _clk = dspsim::Model::create<dspsim::Input<uint8_t>>(clk, top->clk, id(), "clk");
        _rst = dspsim::Model::create<dspsim::Input<uint8_t>>(rst, top->rst, id(), "rst");
        _i = dspsim::Model::create<dspsim::Input<uint8_t>>(i, top->i, id(), "i");
        _o = dspsim::Model::create<dspsim::Output<uint8_t>>(o, top->o, id(), "o");

        this->_kind = "SimpleModel";
    }

    static auto create(
        dspsim::SignalPtr<uint8_t> clk,
        dspsim::SignalPtr<uint8_t> rst,
        dspsim::SignalPtr<uint8_t> i,
        dspsim::SignalPtr<uint8_t> o,
        const std::string &name = "")
    {
        return dspsim::Model::create<SimpleModel>(clk, rst, i, o, name);
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
        .def("trace", &SimpleModel::trace,
             nanobind::arg("trace_path"),
             nanobind::arg("levels") = 99,
             nanobind::arg("options") = 0)
        .def("close", &SimpleModel::close);
}
