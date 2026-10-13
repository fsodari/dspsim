#pragma once
#include <dspsim/dspsim.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace demo
{
    /// A complex sample.
    struct Iq
    {
        int16_t re = 0;
        int16_t im = 0;

        bool operator==(const Iq &) const = default;
    };
    DSPSIM_STRUCT(Iq, re, im)

    /// A bus: numbers of several sizes (with padding between them), a nested struct and an array.
    struct Bus
    {
        uint16_t tag = 0;
        bool valid = false;
        uint32_t data = 0;
        Iq iq;
        std::array<uint8_t, 3> taps{};
        double gain = 0;

        bool operator==(const Bus &) const = default;
    };
    DSPSIM_STRUCT(Bus, tag, valid, data, iq, taps, gain)

    /// A struct that Python cannot see into: it has no DSPSIM_STRUCT.
    struct Opaque
    {
        int count = 0;

        bool operator==(const Opaque &) const = default;
    };

    using Frame = std::array<int16_t, 4>;

    /// Ports with a struct, an array and an array of structs as their values.
    struct ValueStage : public dspsim::Module
    {
        dspsim::Input<Bus> bus_in{"bus_in"};
        dspsim::Output<Bus> bus_out{"bus_out"};
        /// The sum and the difference of the parts of the sample of the bus.
        dspsim::Output<Iq> sum{"sum"};
        /// An array as the value of one port.
        dspsim::Input<Frame> frame_in{"frame_in"};
        dspsim::Output<Frame> frame_out{"frame_out"};
        /// Arrays of ports with a struct as their value.
        dspsim::InputArray<Iq> samples{"samples", {2, 3}};
        dspsim::OutputArray<Iq> conjugates{"conjugates", {2, 3}};
        /// A number that the framework has no classes for.
        dspsim::Input<float> scale{"scale"};
        dspsim::Output<bool> flag{"flag"};
        /// A struct without a description.
        dspsim::Input<Opaque> opaque_in{"opaque_in"};
        dspsim::Output<Opaque> opaque_out{"opaque_out"};
        /// An ordinary port.
        dspsim::Output<uint32_t> evaluations{"evaluations"};

        ValueStage(dspsim::ModuleName name) : Module(name)
        {
            DSPSIM_METHOD(eval)->always("*");
        }

        void eval()
        {
            const Bus &in = bus_in.read();
            Bus out = in;
            out.tag = static_cast<uint16_t>(in.tag + 1);
            out.valid = !in.valid;
            out.data = in.data * 2;
            out.iq = {in.iq.im, in.iq.re};
            out.taps = {in.taps[2], in.taps[1], in.taps[0]};
            out.gain = in.gain * scale.read();
            bus_out.write(out);
            sum.write({static_cast<int16_t>(in.iq.re + in.iq.im), static_cast<int16_t>(in.iq.re - in.iq.im)});

            Frame frame = frame_in.read();
            for (auto &sample : frame)
            {
                sample = static_cast<int16_t>(-sample);
            }
            frame_out.write(frame);

            for (std::size_t i = 0; i < samples.size(); ++i)
            {
                const Iq &sample = samples.flat(i).read();
                conjugates.flat(i).write({sample.re, static_cast<int16_t>(-sample.im)});
            }
            flag.write(scale.read() > 1);
            opaque_out.write({opaque_in.read().count + 1});
            evaluations.write(++count_);
        }

    private:
        uint32_t count_ = 0;
    };
} // namespace demo
