#pragma once
#include "pch.h"
namespace REX::W32 {
    inline void* GetModuleHandleW(const wchar_t*) { return TestEngine::moduleAvailable ? reinterpret_cast<void*>(1) : nullptr; }
    inline void* GetProcAddress(void*, const char*) { return TestEngine::persistenceExport; }
}
