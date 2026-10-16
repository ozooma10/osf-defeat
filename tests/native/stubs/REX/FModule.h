#pragma once
#include "pch.h"
namespace REX {
    struct FModule {
        static FModule GetExecutingModule() { return {}; }
        std::string GetFileName() const { return TestEngine::executable.string(); }
    };
}
