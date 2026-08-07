#pragma once

#include "pch.h"

namespace OSFDefeat::RuntimeState
{
    // Mutating framework operations run on the game thread, while persistence snapshots may run
    // elsewhere. A recursive mutex keeps each composite operation atomic to Save without making
    // nested operations (capture -> defeat, rescue -> capture cleanup) deadlock.
    [[nodiscard]] std::recursive_mutex& Mutex();

    // Deferred work captures the current epoch. Revert advances it immediately, so work queued by
    // the previous save becomes a harmless no-op even before the game-thread reset reaches the queue.
    [[nodiscard]] std::uint64_t CurrentEpoch();
    [[nodiscard]] std::uint64_t AdvanceEpoch();
    [[nodiscard]] bool IsCurrent(std::uint64_t a_epoch);
}
