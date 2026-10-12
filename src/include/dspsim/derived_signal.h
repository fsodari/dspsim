#pragma once
#include <dspsim/signal.h>
#include <dspsim/bitsel.h>
#include <dspsim/channel.h>

#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>

namespace dspsim
{
    /*
        Interface for signals whose value is derived from the bits of other signals.
        Source signals schedule their dependents on the context when they update, and the context
        recomputes them with commit() once all signals in the delta cycle have committed, so a derived
        signal changes in the same delta cycle as its sources and sees a consistent snapshot of them.
    */
    class DerivedSignalBase : public detail::Updatable
    {
        // Sources refresh their derived signals when they are initialized.
        friend class SignalBase;

    public:
        ~DerivedSignalBase() override;

    private:
        // commit() (from detail::Updatable) recomputes the value from the source signals and notifies
        // events if it changed. The scheduler calls it in the update phase.

        /// Recompute the value from the source signals without notifying events. Called when a source is initialized.
        virtual void refresh() = 0;
    };

    /*
        A signal of type T that is a view of a bit selection (a slice or pack of other signals).
        Reads see the selected bits of the sources, zero or sign extended to T. Writes go through to the
        source signals at write time (Signal<T>::write checks source_selection()), so they combine with other
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

        // Convert the selected bits to T, sign extending from the signal's width for signed types.
        T from_bits(uint64_t bits) const;

    private:
        BitSel selection_;
    };
} // namespace dspsim
