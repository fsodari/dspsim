#pragma once
#include <dspsim/signal.h>
#include <dspsim/bitsel.h>
#include <dspsim/detail/derived_signal_base.h>

#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>

namespace dspsim
{
    /*
        A signal of type T that is a view of a bit selection (a slice or pack of other signals).
        Reads see the selected bits of the sources, zero or sign extended to T. Writes go through to the
        source signals at write time (Signal<T>::write() checks for a derived signal), so they combine with other
        writes to the sources in the same cycle, and the new value is read back from them in the update phase.
        Ports bind to a BitSel through a DerivedSignal created by Input::bind / Output::bind.

        Derived signals are owned by the context. They do not unregister from their sources, so the
        sources must outlive the simulation.
    */
    template <typename T>
    class DerivedSignal final : public Signal<T>, public DerivedSignalBase
    {
        static_assert(std::is_integral_v<T>, "Derived signals must have an integral type.");

    public:
        /// Create a derived signal of the selection. The selection must fit in T.
        DerivedSignal(const std::string &name, const BitSel &selection);

        /// Create a derived signal owned by the context.
        static std::shared_ptr<DerivedSignal<T>> create(const std::string &name, const BitSel &selection);

        /// The selection this signal is derived from.
        const BitSel &selection() const;

    private:
        void commit() override;
        void refresh() override;
        void write_through(const void *value) override;
        const BitSel *source_selection() const override;

        // Convert the selected bits to T, sign extending from the signal's width for signed types.
        T from_bits(uint64_t bits) const;

    private:
        BitSel selection_;
    };
} // namespace dspsim
