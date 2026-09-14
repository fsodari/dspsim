#pragma once
#include <dspsim/vmodel.h>

// Include verilated model header
#include <VSkid.h>

// nanobind for binding function.
#include <nanobind/nanobind.h>

// Include appropriate trace header.
#include <verilated_vcd_c.h>


class Skid : public dspsim::VModel<VSkid, VerilatedVcdC>
{
protected:
    // Ports
    dspsim::InputPtr<uint8_t> _clk;
    dspsim::InputPtr<uint8_t> _rst;
    dspsim::InputPtr<uint32_t> _s_axis_tdata;
    dspsim::InputPtr<uint8_t> _s_axis_tvalid;
    dspsim::OutputPtr<uint8_t> _s_axis_tready;
    dspsim::OutputPtr<uint32_t> _m_axis_tdata;
    dspsim::OutputPtr<uint8_t> _m_axis_tvalid;
    dspsim::InputPtr<uint8_t> _m_axis_tready;

public:
    // Parameters
    Skid(
        dspsim::SignalPtr<uint8_t> clk,
        dspsim::SignalPtr<uint8_t> rst,
        dspsim::SignalPtr<uint32_t> s_axis_tdata,
        dspsim::SignalPtr<uint8_t> s_axis_tvalid,
        dspsim::SignalPtr<uint8_t> s_axis_tready,
        dspsim::SignalPtr<uint32_t> m_axis_tdata,
        dspsim::SignalPtr<uint8_t> m_axis_tvalid,
        dspsim::SignalPtr<uint8_t> m_axis_tready,
        const std::string &name = "")
        : VModel<VSkid, VerilatedVcdC>(name)
    {
        _clk = dspsim::Model::create<dspsim::Input<uint8_t>>(clk, top->clk, id(), "clk");
        _rst = dspsim::Model::create<dspsim::Input<uint8_t>>(rst, top->rst, id(), "rst");
        _s_axis_tdata = dspsim::Model::create<dspsim::Input<uint32_t>>(s_axis_tdata, top->s_axis_tdata, id(), "s_axis_tdata");
        _s_axis_tvalid = dspsim::Model::create<dspsim::Input<uint8_t>>(s_axis_tvalid, top->s_axis_tvalid, id(), "s_axis_tvalid");
        _s_axis_tready = dspsim::Model::create<dspsim::Output<uint8_t>>(s_axis_tready, top->s_axis_tready, id(), "s_axis_tready");
        _m_axis_tdata = dspsim::Model::create<dspsim::Output<uint32_t>>(m_axis_tdata, top->m_axis_tdata, id(), "m_axis_tdata");
        _m_axis_tvalid = dspsim::Model::create<dspsim::Output<uint8_t>>(m_axis_tvalid, top->m_axis_tvalid, id(), "m_axis_tvalid");
        _m_axis_tready = dspsim::Model::create<dspsim::Input<uint8_t>>(m_axis_tready, top->m_axis_tready, id(), "m_axis_tready");

        this->_kind = "Skid";
    }

    static auto create(
        dspsim::SignalPtr<uint8_t> clk,
        dspsim::SignalPtr<uint8_t> rst,
        dspsim::SignalPtr<uint32_t> s_axis_tdata,
        dspsim::SignalPtr<uint8_t> s_axis_tvalid,
        dspsim::SignalPtr<uint8_t> s_axis_tready,
        dspsim::SignalPtr<uint32_t> m_axis_tdata,
        dspsim::SignalPtr<uint8_t> m_axis_tvalid,
        dspsim::SignalPtr<uint8_t> m_axis_tready,
        const std::string &name = "")
    {
        return dspsim::Model::create<Skid>(
            clk,
            rst,
            s_axis_tdata,
            s_axis_tvalid,
            s_axis_tready,
            m_axis_tdata,
            m_axis_tvalid,
            m_axis_tready,
            name);
    }
};

static auto bind_Skid(nanobind::module_ &_m)
{
    return nanobind::class_<Skid, dspsim::Model>(_m, "Skid")
        .def(nanobind::new_(&Skid::create),
            nanobind::arg("clk"),
            nanobind::arg("rst"),
            nanobind::arg("s_axis_tdata"),
            nanobind::arg("s_axis_tvalid"),
            nanobind::arg("s_axis_tready"),
            nanobind::arg("m_axis_tdata"),
            nanobind::arg("m_axis_tvalid"),
            nanobind::arg("m_axis_tready"),
             nanobind::kw_only(),
             nanobind::arg("name") = "")
        // .def_prop_rw("name", &Skid::name, &Skid::set_name)
        .def("trace", &Skid::trace,
             nanobind::arg("trace_path"),
             nanobind::arg("levels") = 99,
             nanobind::arg("options") = 0)
        .def("close", &Skid::close);
}