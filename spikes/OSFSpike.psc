Scriptname OSFSpike Hidden
{ [SPIKE] Native declarations bound by OSF Defeat's src/Harness/Spike.cpp. Throwaway PoC. }

; --- Spike A: native global + game-thread marshaling probe.
bool Function Ping(string asTag) global native

; --- Spike B: hand a (quest, actor) pair from Papyrus into C++. C++ pushes them
; back into content Papyrus (OSFSpikeContent.Fill) via DispatchStaticCall, which
; performs the alias ForceRefTo. Returns true if the native accepted the call.
bool Function RunHandoff(Quest akQuest, Actor akTarget) global native
