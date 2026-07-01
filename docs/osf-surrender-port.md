# OSF Surrender Port Handoff

OSF Defeat owns the neutral defeat state. OSF Surrender should consume it as a content/consequence pack.

## Recommended Flow

1. OSF Surrender ships a player consequence JSON under `SFSE/OSFDefeat/Consequences/Player/`.
2. When OSF Defeat selects that consequence, it **starts the `quest`** (VM-dispatched
   `Quest.Start`; editorID, or hex FormID) and, with `holdRecovery`, enters the down-state
   already held — no auto-recovery race. Then `OnPlayerConsequenceStarted` fires.
3. OSF Surrender (the started quest, or a registered `OnPlayerConsequenceStarted` receiver)
   reads the payload through the native getters — aggressor, participants, recovery.
4. For a full surrender-to-base beat, OSF Surrender calls
   `OSFDefeat.ApplyCaptiveFaction(Game.GetPlayer())` (all hostiles go non-hostile), and starts
   its authored scene through OSF Animation.
5. When its content is done, OSF Surrender calls `OSFDefeat.RescueActor(Game.GetPlayer(), True)`
   (which also removes the captive faction), or hands control back with
   `OSFDefeat.SetRecoveryTimer(Game.GetPlayer(), seconds)`.

With `holdRecovery` the down-state is auto-claimed under the consequence's name and watchdog-
protected: if the scene runs longer than `maxHoldSeconds` (or the config `watchdogSeconds`,
default 600s), extend the lease with `OSFDefeat.ClaimDefeat(Game.GetPlayer(), "<consequence name>")`
before it lapses — a lapsed deadline force-rescues the player and logs the consequence as the
defaulter. That is the framework's guarantee that a broken scene can never strand the player.

## Consequence JSON Example

```json
{
  "version": 1,
  "name": "OSF Surrender Human Scene",
  "priority": 100,
  "weight": 1,
  "recoverySeconds": 120.0,
  "quest": "OSFSurrenderHumanScene",
  "holdRecovery": true
}
```

If `quest` resolves to a real quest it is started automatically (`"startQuest": false` opts out);
an unresolvable token stays a plain content string in the handoff context.

## Papyrus Consumer Sketch

```papyrus
Scriptname OSFSurrenderManager Extends Quest

Event OnInit()
    OSFDefeat.RegisterForPlayerConsequenceStarted(Self)
EndEvent

Event OnPlayerConsequenceStarted()
    String consequence = OSFDefeat.GetLastPlayerConsequenceName()
    String questToken = OSFDefeat.GetLastPlayerConsequenceQuest()
    Actor aggressor = OSFDefeat.GetLastPlayerAggressor()
    Actor[] participants = OSFDefeat.GetLastPlayerParticipants()
    Float recoverySeconds = OSFDefeat.GetLastPlayerRecoverySeconds()

    If questToken == "OSFSurrenderHumanScene"
        StartHumanSurrenderScene(Game.GetPlayer(), aggressor, recoverySeconds)
    EndIf
EndEvent

Function StartHumanSurrenderScene(Actor akPlayer, Actor akAggressor, Float afRecoverySeconds)
    ; OSF Surrender owns OSF Animation calls, voice/haptics, stripping,
    ; ransom/resist/capture/bad-end routing, and content cleanup.
EndFunction
```

## Ownership Boundary

OSF Defeat should keep:

- death prevention
- defeated/recovered state
- recovery persistence + the hold/reschedule handshake (`SetRecoveryTimer`)
- processing gates
- consequence selection + quest start
- handoff context (aggressor, participants, recovery)
- captive-faction pacify mechanics (`ApplyCaptiveFaction` / `RemoveCaptiveFaction`)
- generic cleanup protection

OSF Surrender should keep:

- threshold surrender policy, if it wants pre-lethal surrender
- human/creature routing
- OSF Animation scenes
- voice/haptic/timeline metadata
- stripping/dressing choices
- ransom/resist/capture/bad-end/arrest outcomes
- robbery/capture/captive/device systems