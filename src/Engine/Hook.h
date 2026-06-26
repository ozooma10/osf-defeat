#pragma once

#include "pch.h"

#include "REL/ASM.h"

#include <array>
#include <cstring>
#include <format>

// =============================================================================
// Engine::Hook — the plugin's one piece of hooking machinery. Ported from the OSF
// RE sandbox's Util::Hooking::InstallEntryHookWithGateway (live-proven on 1.16.244):
//
//   1. Verify the function's prologue bytes (drift / wrong-ID guard).
//   2. Steal the first StealN bytes (must be a clean instruction boundary, >= 5) into
//      a trampoline-allocated "gateway", followed by a 14-byte absolute JMP back to
//      entry+StealN. Calling the gateway therefore runs the original function intact.
//   3. Overwrite the entry with a 5-byte JMP to the detour (CommonLibSF routes through a
//      branch stub when the detour is out of rel32 range).
//
// Requires the trampoline to be allocated first — SFSE::Init(.., {.trampoline = true,
// .trampolineSize = N}) in main.cpp.
// =============================================================================
namespace OSFDefeat::Engine::Hook
{
	namespace detail
	{
		[[nodiscard]] inline bool IsReadable(std::uintptr_t a_addr, std::size_t a_size)
		{
			if (a_addr == 0 || a_size == 0) {
				return false;
			}
			MEMORY_BASIC_INFORMATION mbi{};
			if (::VirtualQuery(reinterpret_cast<LPCVOID>(a_addr), &mbi, sizeof(mbi)) == 0) {
				return false;
			}
			if (mbi.State != MEM_COMMIT) {
				return false;
			}
			const auto protect = mbi.Protect & 0xFF;
			return (mbi.Protect & PAGE_GUARD) == 0 && protect != PAGE_NOACCESS;
		}
	}

	// Verify VerifyN expected bytes at a_addr; logs the actual bytes on mismatch and
	// returns false so the caller can self-disable instead of patching a drifted target.
	template <std::size_t VerifyN>
	[[nodiscard]] inline bool VerifyBytes(
		const char* a_label, std::uintptr_t a_addr, const std::array<std::uint8_t, VerifyN>& a_expected)
	{
		if (!detail::IsReadable(a_addr, VerifyN)) {
			REX::WARN("[OSFDefeat] {}: address {:#x} unreadable - hook skipped", a_label, a_addr);
			return false;
		}
		const auto* const actual = reinterpret_cast<const std::uint8_t*>(a_addr);
		if (std::memcmp(actual, a_expected.data(), VerifyN) == 0) {
			return true;
		}
		std::string got;
		for (std::size_t i = 0; i < VerifyN; ++i) {
			got += std::format("{}{:02X}", i ? " " : "", actual[i]);
		}
		REX::WARN("[OSFDefeat] {}: prologue drifted at {:#x} ({}) - hook skipped", a_label, a_addr, got);
		return false;
	}

	// Install an entry detour bound by raw AddrLib ID (the EngineFns idiom — this
	// CommonLibSF copy isn't synced, so we don't rely on modelled relocations). Steals
	// StealN bytes, verifies VerifyN (>= StealN). Returns the gateway (call it to run the
	// original) or 0 on any failure.
	template <std::size_t StealN, std::size_t VerifyN, class F>
	[[nodiscard]] inline std::uintptr_t InstallEntry(
		std::uint64_t a_id, const char* a_label, const std::array<std::uint8_t, VerifyN>& a_verify, F a_thunk)
	{
		static_assert(StealN >= sizeof(REL::ASM::JMP5), "need >= 5 bytes to write the detour jmp");
		static_assert(VerifyN >= StealN, "verify window must cover the stolen bytes");

		REL::Relocation<std::uintptr_t> reloc{ REL::ID(a_id) };
		const auto                      entry = reloc.address();
		if (!VerifyBytes(a_label, entry, a_verify)) {
			return 0;
		}

		auto&       trampoline = REL::GetTrampoline();
		auto* const gateway = static_cast<std::byte*>(trampoline.allocate(StealN + sizeof(REL::ASM::JMP14)));
		std::memcpy(gateway, reinterpret_cast<const void*>(entry), StealN);
		const REL::ASM::JMP14 jumpBack{ entry + StealN };
		std::memcpy(gateway + StealN, &jumpBack, sizeof(jumpBack));

		reloc.write_jmp<5>(a_thunk);
		return reinterpret_cast<std::uintptr_t>(gateway);
	}
}
