#pragma once
#include "pch.h"
namespace REL {
    struct ID {
        std::uint64_t value; constexpr explicit ID(std::uint64_t id) : value(id) {}
        std::uintptr_t address() const { throw std::runtime_error("Address lookup is not an offline test"); }
    };
    template<class T> struct Relocation {
        explicit Relocation(ID id) { if (id.value != 810305) throw std::runtime_error("Unexpected relocation in offline test"); }
        const std::uint8_t& operator*() const { return TestEngine::queueEnabled; }
    };
}
