/*
    Channel: user-defined state that is committed in the update phase (request_update() / update()).
*/
#include <dspsim/dspsim.h>

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace dspsim;

namespace
{
    // A value that is written in one delta cycle and can be read in the next, with an event for the readers.
    class Mailbox : public Channel
    {
    public:
        explicit Mailbox(Context *context) : Channel(context), written(context) {}

        void put(int value)
        {
            next_ = value;
            request_update();
        }
        int get() const { return value_; }

        SensitivityEvent written;
        int updates = 0;

    protected:
        void update() override
        {
            value_ = next_;
            ++updates;
            written.notify();
        }

    private:
        int value_ = 0;
        int next_ = 0;
    };

    Task<> writer(Mailbox *mailbox, int value)
    {
        mailbox->put(value - 1);
        // Requested twice: updated once, with the last value.
        mailbox->put(value);
        co_return;
    }

    // Records what it reads when it starts, and again every time the mailbox was written.
    Task<> reader(Context *context, Mailbox *mailbox, std::vector<int> *seen)
    {
        while (true)
        {
            seen->push_back(mailbox->get());
            co_await context->wait(mailbox->written);
        }
    }
} // namespace

TEST_CASE("a channel is updated in the update phase of the delta cycle that requested it", "[channel]")
{
    // The writer and the reader start in the same delta cycle, in either order. The reader must see the
    // old value in that delta cycle and the new one in the next.
    for (bool writer_first : {true, false})
    {
        auto ctx = Context::create();
        Mailbox mailbox{ctx.get()};
        std::vector<int> seen;
        if (writer_first)
        {
            ctx->register_coro_task(writer(&mailbox, 7), "writer");
            ctx->register_coro_task(reader(ctx.get(), &mailbox, &seen), "reader");
        }
        else
        {
            ctx->register_coro_task(reader(ctx.get(), &mailbox, &seen), "reader");
            ctx->register_coro_task(writer(&mailbox, 7), "writer");
        }
        ctx->elaborate();
        ctx->run(0);

        REQUIRE(seen == std::vector<int>{0, 7});
        REQUIRE(mailbox.updates == 1);
    }
}

TEST_CASE("a channel update requested between runs happens in the next run", "[channel]")
{
    auto ctx = Context::create();
    Mailbox mailbox{ctx.get()};
    std::vector<int> seen;
    ctx->register_coro_task(reader(ctx.get(), &mailbox, &seen), "reader");
    ctx->elaborate();
    ctx->run(10);
    REQUIRE(seen == std::vector<int>{0});

    // Nothing else is scheduled: the request alone has to start a delta cycle.
    mailbox.put(3);
    REQUIRE(mailbox.get() == 0);
    ctx->run(0);
    REQUIRE(mailbox.get() == 3);
    REQUIRE(seen == std::vector<int>{0, 3});

    ctx->run(10);
    REQUIRE(mailbox.updates == 1);
}
