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
        Interface for signals whose value is derived from other signals: from a selection of their bits
        (DerivedSignal) or from a field of the struct that is their value (FieldSignal).
        Source signals schedule their dependents on the context when they update, and the context
        recomputes them with commit() once all signals in the delta cycle have committed, so a derived
        signal changes in the same delta cycle as its sources and sees a consistent snapshot of them.
        What is written to a derived signal goes to its sources (write_through()).
    */
    class DerivedSignalBase : public detail::Updatable
    {
        // Sources refresh their derived signals when they are initialized, and pass on what is written to
        // a derived signal.
        friend class SignalBase;

    public:
        ~DerivedSignalBase() override;

    protected:
        /// Have commit() called in the update phase of every delta cycle in which the source changes.
        void depend_on(SignalBase &source);
        /// Schedule the update of a source whose pending value this signal has changed.
        static void request_update(SignalBase &source);

    private:
        // commit() (from detail::Updatable) recomputes the value from the source signals and notifies
        // events if it changed. The scheduler calls it in the update phase.

        /// Recompute the value from the source signals without notifying events. Called when a source is initialized.
        virtual void refresh() = 0;
        /// Write a value of the signal's type to the source signals. Signal<T>::write() ends up here.
        virtual void write_through(const void *value) = 0;
        /// The bit selection that the signal is a view of, or nullptr.
        virtual const BitSel *source_selection() const;
        /// The field that the signal is a view of. The signal is nullptr if it is not a view of a field.
        virtual detail::FieldRoot field_root();
        /// This signal, if it is the view of the field at `q` in the committed value of its source. nullptr
        /// otherwise.
        virtual SignalBase *field_signal(const void *q);
    };

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
