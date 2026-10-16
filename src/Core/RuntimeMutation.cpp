#include "pch.h"

#include "Core/RuntimeMutation.h"

namespace OSFDefeat::RuntimeMutation
{
    namespace
    {
        std::recursive_mutex g_mutex;
        std::atomic_uint64_t g_epoch{ 1 };
    }

    std::recursive_mutex& Mutex() { return g_mutex; }

    RE::BSService::TaskQueue* GetTaskQueue()
    {
        // g_bUseTaskQueue, confirmed on 1.16.244 in OSF RE's engine.main_thread_task_queue.
        // This pinned CommonLibSF exposes AddTask but not the queue-enabled accessor yet.
        static REL::Relocation<const std::uint8_t*> enabled{ REL::ID{ 810305 } };
        return *enabled == 1 ? RE::BSService::TaskQueue::GetSingleton() : nullptr;
    }

    std::uint64_t CurrentEpoch() { return g_epoch.load(std::memory_order_acquire); }
    std::uint64_t AdvanceEpoch() { return g_epoch.fetch_add(1, std::memory_order_acq_rel) + 1; }
    bool IsCurrent(std::uint64_t a_epoch) { return CurrentEpoch() == a_epoch; }
}
