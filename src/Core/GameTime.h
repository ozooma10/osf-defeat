#pragma once

#include "pch.h"

namespace OSFDefeat::GameTime
{
    [[nodiscard]] inline float NowDay()
    {
        const auto* const calendar = RE::Calendar::GetSingleton();
        return calendar ? calendar->GetDaysPassedExact() : 0.0f;
    }
}
