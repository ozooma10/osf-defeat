#pragma once

// =============================================================================
// Papyrus — the framework's Papyrus API (the Acheron-style content seam). OSF
// Defeat owns the C++ defeat lifecycle; content mods are ordinary Papyrus quest
// mods that call these natives. A consumer detects its own trigger (e.g.
// RegisterForHitEvent + a Health check, as SAF_Surrender does) and calls
// DefeatActor; the actor is HELD until the consumer calls RescueActor.
//
// Native functions are registered on the VM (RE::GameVM) under the script class
// "OSFDefeat"; the matching declarations live in dist/Scripts/Source/OSFDefeat.psc.
//
// State-mutating verbs are marshalled onto the game thread (the VM may call a
// native off-thread); read-only queries run inline.
// =============================================================================
namespace OSFDefeat::Papyrus
{
	// Bind the native functions on the Papyrus VM. Call once after data load.
	void Register();
}
