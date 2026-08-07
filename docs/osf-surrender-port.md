# OSF Surrender Port Handoff

OSF Defeat owns the neutral defeat state. OSF Surrender should consume it as a content/consequence pack.

## Recommended Flow

1. OSF Surrender ships a player consequence JSON under `SFSE/OSFDefeat/Consequences/Player/`.
2. When OSF Defeat selects that consequence, it **starts the `quest`** (VM-dispatched
   `Quest.Start`; editorID, or hex FormID) and, with `recoveryMode: "held"`, enters the down-state
   already held — no auto-recovery race. Then `OnPlayerConsequenceStarted` fires.
3. OSF Surrender (the started quest, or a registered `OnPlayerConsequenceStarted` receiver)
   reads the payload through the native getters — consequence ID, aggressor, recent hostile attackers, and recovery mode.
4. For a full surrender-to-base beat, OSF Surrender calls
   `OSFDefeat.ApplyCaptiveFaction(Game.GetPlayer())` (all hostiles go non-hostile), and starts
   its authored scene through OSF Animation.
5. When its content is done, OSF Surrender calls `OSFDefeat.RecoverActor(Game.GetPlayer(), True)`
   (which also removes the captive faction), or hands control back with
   `OSFDefeat.ScheduleAutomaticRecovery(Game.GetPlayer(), seconds)`.

With held recovery, the down-state is automatically leased under `consequence:<id>` and watchdog-
protected. If the scene runs longer than `leaseTimeoutSeconds` (or the settings `watchdogSeconds`,
default 600s), extend the lease with `OSFDefeat.AcquireDefeatLease(Game.GetPlayer(), "consequence:<id>")`
before it lapses. A lapsed deadline force-recovers the player and logs the consequence ID as the
defaulter. That is the framework's guarantee that a broken scene can never strand the player.

## Consequence JSON Example

```json
{
  "version": 3,
  "id": "osfsurrender.human_scene",
  "displayName": "OSF Surrender Human Scene",
  "priority": 100,
  "weight": 1,
  "autoRecoveryDelaySeconds": 120.0,
  "quest": "OSFSurrenderHumanScene",
  "recoveryMode": "held",
  "leaseTimeoutSeconds": 600.0
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
    String consequenceId = OSFDefeat.GetLastPlayerConsequenceId()
    String questToken = OSFDefeat.GetLastPlayerConsequenceQuest()
    Actor aggressor = OSFDefeat.GetLastPlayerAggressor()
    Actor[] recentHostileAttackers = OSFDefeat.GetLastPlayerRecentHostileAttackers()
    Bool recoveryHeld = OSFDefeat.IsLastPlayerRecoveryHeld()

    If consequenceId == "osfsurrender.human_scene" && recoveryHeld
        StartHumanSurrenderScene(Game.GetPlayer(), aggressor, recentHostileAttackers)
    EndIf
EndEvent

Function StartHumanSurrenderScene(Actor akPlayer, Actor akAggressor, Actor[] akRecentHostileAttackers)
    ; OSF Surrender owns OSF Animation calls, voice/haptics, stripping,
    ; ransom/resist/capture/bad-end routing, and content cleanup.
EndFunction
```

## Ownership Boundary

OSF Defeat should keep:

- death prevention
- defeated/recovered state
- recovery persistence + explicit hold/schedule verbs
- processing gates
- consequence selection + quest start
- handoff context (consequence ID, aggressor, recent hostile attackers, recovery mode)
- captive-faction non-hostility mechanics (`ApplyCaptiveFaction` / `RemoveCaptiveFaction`)
- generic cleanup protection

OSF Surrender should keep:

- threshold surrender policy, if it wants pre-lethal surrender
- human/creature routing
- OSF Animation scenes
- voice/haptic/timeline metadata
- stripping/dressing choices
- ransom/resist/capture/bad-end/arrest outcomes
- robbery/capture/captive/device systems
