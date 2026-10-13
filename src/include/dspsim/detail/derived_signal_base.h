#pragma once
#include <dspsim/channel.h>
#include <dspsim/value_type.h>

// DerivedSignalBase is in a header of its own because both kinds of derived signal need it, and one of them,
// FieldSignal, is included by <dspsim/signal.h> itself: this header needs nothing of the signals.
namespace dspsim
{
    class BitSel;
    class SignalBase;

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
        // A signal of a struct finds the signals of its fields among its derived signals.
        friend class RecordSignalBase;

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
} // namespace dspsim
