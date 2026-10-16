#pragma once
#include "RE/B/BSService.h"
namespace REL::ASM {
    struct JMP14 { std::uintptr_t target; };
    struct JMP5 { JMP5(std::uintptr_t, std::uintptr_t) {} std::uint8_t bytes[5]; };
}
