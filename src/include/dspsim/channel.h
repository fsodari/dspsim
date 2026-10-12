#pragma once

namespace dspsim
{
    class Context;
    template <typename T>
    class FlaggedStack;

    namespace detail
    {
        /*
            Something the context calls at the end of the update phase, once the signals have committed
            their new values: a derived signal that is recomputed from its sources, or a channel that
            commits its changes.
        */
        class Updatable
        {
            // The scheduler calls commit(), and its stack keeps the "is scheduled" flag in the object.
            friend class dspsim::Context;
            template <typename T>
            friend class dspsim::FlaggedStack;

        public:
            virtual ~Updatable();

        private:
            virtual void commit() = 0;
            // Inline: checked by the scheduler whenever a source signal updates.
            bool &scheduled_flag() { return scheduled_; }

        private:
            bool scheduled_ = false;
        };
    } // namespace detail

    /*
        Base class of a user-defined channel: state that processes change during the evaluation phase and
        that becomes visible to them in the update phase, the way the value of a signal does. Every process
        of a delta cycle then sees the same state, whatever the order in which they are evaluated.
        (SystemC: sc_prim_channel.)

        A channel calls request_update() when it has something to commit. The context calls update() once
        in the update phase of the current delta cycle, after the signals have committed their values,
        however often the update was requested. update() typically moves what was written into what can
        be read, and notifies an event for the processes that wait for it:

            template <typename T>
            class Mailbox : public dspsim::Channel
            {
            public:
                explicit Mailbox(dspsim::Context *context) : Channel(context), written(context) {}

                void put(const T &value)
                {
                    next_ = value;
                    request_update();
                }
                const T &get() const { return value_; }

                dspsim::SensitivityEvent written;

            protected:
                void update() override
                {
                    value_ = next_;
                    written.notify();
                }

            private:
                T value_{};
                T next_{};
            };

        update() runs in the update phase, when the processes of the delta cycle have been evaluated and
        what they wrote is being committed. It may change the channel's own state and notify events. It
        must not write to a signal and must not request the update of a channel, itself included: a
        write belongs to the evaluation phase, and one made here is not committed with the others of the
        delta cycle. This is a rule for the author of a channel; the engine does not check it.

        A channel must stay alive until an update it has requested has run.
    */
    class Channel : public detail::Updatable
    {
    public:
        /// A channel belongs to a context. Like an event, it can be created at any time.
        explicit Channel(Context *context);

        Context *context() const { return context_; }

    protected:
        /*
            Ask for update() to be called in the update phase of the current delta cycle. When it is called
            from outside the simulation, between two runs, the update runs at the start of the next run.
        */
        void request_update();

        /// Commit the changes made since the last update. Called by the context in the update phase.
        /// Must not write to a signal or request the update of a channel (see above).
        virtual void update() = 0;

    private:
        void commit() final;

    private:
        Context *context_;
    };
} // namespace dspsim
