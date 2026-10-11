#include <dspsim/detail/engine_access.h>
#include <dspsim/context.h>
#include <dspsim/process.h>

namespace dspsim::detail
{
    ProcessBase *EngineAccess::add_process(Context &context, std::unique_ptr<ProcessBase> process)
    {
        return context.add_process(std::move(process));
    }

    void EngineAccess::remove_process(Context &context, ProcessBase *process)
    {
        context.remove_process(process);
    }

    void EngineAccess::run_until_done(Context &context, ProcessBase *process, uint64_t timeout)
    {
        context.run_until_done(process, timeout);
    }

    ProcessBase *EngineAccess::current_process(const Context &context)
    {
        return context.current_process();
    }

    std::coroutine_handle<> EngineAccess::resume_point(const ProcessBase &process)
    {
        return process.resume_point();
    }
} // namespace dspsim::detail
