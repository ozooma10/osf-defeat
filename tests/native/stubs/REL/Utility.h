#pragma once
#include "pch.h"
namespace REL {
    struct Trampoline {
        void* allocate(std::size_t) { throw std::runtime_error("Trampoline allocation is not an offline test"); }
        std::uintptr_t allocate_branch5(std::uintptr_t) { throw std::runtime_error("Trampoline patching is not an offline test"); }
    };
    inline Trampoline& GetTrampoline() { static Trampoline trampoline; return trampoline; }
    template<class T> bool WriteSafeData(std::uintptr_t, const T&) { return false; }
    inline bool WriteSafeFill(std::uintptr_t, int, std::size_t) { return false; }
}
