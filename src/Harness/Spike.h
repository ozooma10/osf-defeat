#pragma once

#include "pch.h"

// =============================================================================
// Spike — two throwaway proof-of-concept probes that answer the questions
// gating the "OSF Defeat -> generic Acheron-style foundation" re-architecture.
// NOT shipping code. Delete src/Harness/Spike.*, the spikes/ folder, and the
// [SPIKE] line in main.cpp once both questions are answered.
//
//   Spike A — can we register a NATIVE Papyrus global from C++ (so a .esp/Papyrus
//             content mod can consume the foundation), and marshal a state-mutating
//             call onto the game thread? Gates the entire Papyrus seam (Phase 1).
//
//   Spike B — can the foundation hand a defeated actor to a CONTENT quest the way
//             Acheron does (fill a quest's reference alias with the victim)?
//             RE finding (from the CommonLibSF copy in lib/): pure-C++ alias
//             force-fill is NOT exposed — BGSRefAlias is RTTI-only, there is no
//             ForceRefTo and no AddrLib id. So Spike B tests the VIABLE path: C++
//             pushes the (quest, victim) refs into content Papyrus via the VM
//             (DispatchStaticCall) and the content script performs the alias fill.
// =============================================================================
namespace OSFDefeat::Harness
{
	class Spike
	{
	public:
		// Bind the native Papyrus globals. Idempotent. Call once when the VM is up
		// (wired to SFSE kPostDataLoad in main.cpp).
		static void Register();
	};
}
