#pragma once

#include "pch.h"

namespace OSFDefeat::Consequence
{
    struct PlayerOutcome
    {
        std::string name{ "Built-in down then recover" };
        int priority{ 0 };
        int weight{ 1 };
        float recoverySeconds{ 8.0f };
        std::optional<std::string> quest;
    };

    void Load();
    void Reload();

    [[nodiscard]] PlayerOutcome SelectPlayerOutcome();
    [[nodiscard]] float DefaultRecoverySeconds();
}