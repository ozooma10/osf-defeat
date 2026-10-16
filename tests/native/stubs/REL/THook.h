#pragma once
#include "RE/B/BSService.h"
namespace REL {
    enum class EHookType { CALL5, CALL6 };
    template<class T> struct THook {
        template<class... Args> explicit THook(Args&&...) {}
        EHookType GetType() const { return EHookType::CALL5; }
        bool Init() { throw std::runtime_error("Hook installation is not an offline test"); }
        bool Enable() { return false; }
        void operator()() {}
    };
}
