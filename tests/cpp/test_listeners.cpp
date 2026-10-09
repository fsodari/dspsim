#include <dspsim/context.h>

#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

namespace
{
    // Context::create() refuses to replace an unelaborated context. Reset the global context after
    // every test case so a test that fails (or never elaborates) can't break the tests after it.
    class ResetContextListener : public Catch::EventListenerBase
    {
    public:
        using Catch::EventListenerBase::EventListenerBase;

        void testCaseEnded(const Catch::TestCaseStats &) override
        {
            dspsim::Context::reset();
        }
    };
} // namespace

CATCH_REGISTER_LISTENER(ResetContextListener)
