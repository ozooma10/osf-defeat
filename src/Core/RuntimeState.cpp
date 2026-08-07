#include "pch.h"

#include "Core/RuntimeState.h"

namespace OSFDefeat::RuntimeState
{
    namespace
    {
        std::recursive_mutex g_mutex;
        std::atomic_uint64_t g_epoch{ 1 };
    }

    std::recursive_mutex& Mutex() { return g_mutex; }
    std::uint64_t CurrentEpoch() { return g_epoch.load(std::memory_order_acquire); }
    std::uint64_t AdvanceEpoch() { return g_epoch.fetch_add(1, std::memory_order_acq_rel) + 1; }
    bool IsCurrent(std::uint64_t a_epoch) { return CurrentEpoch() == a_epoch; }
}
