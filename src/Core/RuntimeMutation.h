#pragma once

#include "pch.h"
#include "RE/B/BSService.h"

namespace OSFDefeat::RuntimeMutation
{
    // Coordinates composite runtime mutations with off-thread persistence snapshots.
    [[nodiscard]] std::recursive_mutex& Mutex();

    // The engine queue may run inline on its drain owner. Reject early boot, when it
    // would instead run inline on any caller. This owns no queue or worker thread.
    [[nodiscard]] RE::BSService::TaskQueue* GetTaskQueue();

    // Deferred work captures the current epoch; save reversion advances it so stale work no-ops.
    [[nodiscard]] std::uint64_t CurrentEpoch();
    [[nodiscard]] std::uint64_t AdvanceEpoch();
    [[nodiscard]] bool IsCurrent(std::uint64_t a_epoch);
}
