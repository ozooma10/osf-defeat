#pragma once

#include "pch.h"

namespace OSFDefeat::PlayerDefeat
{
    void Install();
    void BeginPlayerDefeat(RE::Actor* a_aggressor = nullptr);
    void TickRecovery();

    [[nodiscard]] float RecoveryDeadlineForSeconds(float a_realSeconds);
}