#pragma once
#include <dspsim/vmodel.h>

// Include verilated model header
#include <VHellModel.h>

// nanobind for binding function.
#include <nanobind/nanobind.h>

// Include appropriate trace header.
#include <verilated_vcd_c.h>


class HellModel : public dspsim::VModel<VHellModel, VerilatedVcdC>
{
protected:
    // Ports
    dspsim::InputPtr<uint8_t> _clk;
    dspsim::InputPtr<uint8_t> _rst;
    dspsim::InputPtr<uint32_t> _a;
    dspsim::OutputPtr<uint8_t> _b;
    dspsim::InputPtr<uint8_t> _c;
    dspsim::OutputPtr<uint8_t> _d;
    dspsim::OutputPtr<uint32_t> _e;
    dspsim::OutputPtr<uint32_t> _f;
    dspsim::OutputPtr<uint32_t> _g;

public:
    // Parameters
    HellModel(
        dspsim::SignalPtr<uint8_t> clk,
        dspsim::SignalPtr<uint8_t> rst,
        dspsim::SignalPtr<uint32_t> a,
        dspsim::SignalPtr<uint8_t> b,
        dspsim::SignalPtr<uint8_t> c,
        dspsim::SignalPtr<uint8_t> d,
        dspsim::SignalPtr<uint32_t> e,
        dspsim::SignalPtr<uint32_t> f,
        dspsim::SignalPtr<uint32_t> g,
        const std::string &name = "")
        : VModel<VHellModel, VerilatedVcdC>(name)
    {
        _clk = dspsim::Model::create<dspsim::Input<uint8_t>>(clk, top->clk, id(), "clk");
        _rst = dspsim::Model::create<dspsim::Input<uint8_t>>(rst, top->rst, id(), "rst");
        _a = dspsim::Model::create<dspsim::Input<uint32_t>>(a, top->a, id(), "a");
        _b = dspsim::Model::create<dspsim::Output<uint8_t>>(b, top->b, id(), "b");
        _c = dspsim::Model::create<dspsim::Input<uint8_t>>(c, top->c, id(), "c");
        _d = dspsim::Model::create<dspsim::Output<uint8_t>>(d, top->d, id(), "d");
        _e = dspsim::Model::create<dspsim::Output<uint32_t>>(e, top->e, id(), "e");
        _f = dspsim::Model::create<dspsim::Output<uint32_t>>(f, top->f, id(), "f");
        _g = dspsim::Model::create<dspsim::Output<uint32_t>>(g, top->g, id(), "g");

        this->_kind = "HellModel";
    }

    static auto create(
        dspsim::SignalPtr<uint8_t> clk,
        dspsim::SignalPtr<uint8_t> rst,
        dspsim::SignalPtr<uint32_t> a,
        dspsim::SignalPtr<uint8_t> b,
        dspsim::SignalPtr<uint8_t> c,
        dspsim::SignalPtr<uint8_t> d,
        dspsim::SignalPtr<uint32_t> e,
        dspsim::SignalPtr<uint32_t> f,
        dspsim::SignalPtr<uint32_t> g,
        const std::string &name = "")
    {
        return dspsim::Model::create<HellModel>(
            clk,
            rst,
            a,
            b,
            c,
            d,
            e,
            f,
            g,
            name);
    }
};

static auto bind_HellModel(nanobind::module_ &_m)
{
    return nanobind::class_<HellModel, dspsim::Model>(_m, "HellModel")
        .def(nanobind::new_(&HellModel::create),
            nanobind::arg("clk"),
            nanobind::arg("rst"),
            nanobind::arg("a"),
            nanobind::arg("b"),
            nanobind::arg("c"),
            nanobind::arg("d"),
            nanobind::arg("e"),
            nanobind::arg("f"),
            nanobind::arg("g"),
             nanobind::kw_only(),
             nanobind::arg("name") = "")
        // .def_prop_rw("name", &HellModel::name, &HellModel::set_name)
        .def("trace", &HellModel::trace,
             nanobind::arg("trace_path"),
             nanobind::arg("levels") = 99,
             nanobind::arg("options") = 0)
        .def("close", &HellModel::close);
}