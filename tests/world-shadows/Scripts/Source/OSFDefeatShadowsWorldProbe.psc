Scriptname OSFDefeatShadowsWorldProbe extends Quest
; Test-only add-on. Never gives the subject an additional persistence alias,
; suspends its AI, respawns it, or refreshes its identity stamp after creation.
OSFDefeatShadowsHunt Property Hunt Auto Const Mandatory
ActorValue Property IdentityStamp Auto Const Mandatory

Bool armed = False
Bool detached = False
Bool observedAway = False
Int stamp = 0
Int initialID = 0
Int trips = 0
Int resetEvents = 0
Form expectedBase
Planet originPlanet
Float beganDay
Float awayDay
String siteKind

Function Log(String entry)
    Debug.OpenUserLog("OSFDefeatShadowsWorld")
    Debug.TraceUser("OSFDefeatShadowsWorld", siteKind + "|" + entry)
EndFunction

Bool Function Check(String label, Bool ok)
    If ok
        Log("PASS|" + label)
    Else
        Log("FAIL|" + label)
    EndIf
    Return ok
EndFunction

Function BeginTrial(Actor existingEnemy, String kind)
    If armed || (kind != "authored" && kind != "generated") || existingEnemy == None
        Log("BLOCKED|Provide an existing eligible enemy and authored/generated kind, once per clean seed.")
        Return
    EndIf
    If !Hunt.IsRunning()
        Hunt.Start()
    EndIf
    Actor player = Game.GetPlayer()
    If Hunt.Nemesis.GetReference() != None || !Hunt.Eligible(existingEnemy) || existingEnemy.GetParentCell() != player.GetParentCell() || !existingEnemy.Is3DLoaded()
        Log("BLOCKED|Enemy must be eligible, loaded in this cell, and the hunt slot empty.")
        Return
    EndIf
    If player.GetCurrentPlanet() == None || player.IsInSpaceEx(True) || player.GetCurrentShipRef() != None
        Log("BLOCKED|Begin at a planetary combat site, outside ships.")
        Return
    EndIf
    siteKind = kind
    stamp = Utility.RandomInt(100000, 16000000)
    expectedBase = existingEnemy.GetBaseObject()
    initialID = existingEnemy.GetFormID()
    originPlanet = player.GetCurrentPlanet()
    beganDay = Utility.GetCurrentGameTime()
    existingEnemy.SetValue(IdentityStamp, stamp)
    armed = True
    RegisterForRemoteEvent(Hunt.Nemesis, "OnCellDetach")
    RegisterForRemoteEvent(Hunt.Nemesis, "OnCellAttach")
    RegisterForRemoteEvent(Hunt.Nemesis, "OnReset")
    Log("BEGIN|scripted defeat attributed to existing enemy=" + initialID + "|base=" + expectedBase.GetFormID() + "|stamp=" + stamp + "|planet=" + originPlanet.GetFormID() + "|day=" + beganDay)
    ; Uses the same consequence path as a lethal defeat, without needing to win
    ; a combat-input race. This does not claim a naturally lethal hit occurred.
    OSFDefeat.DefeatActor(player, existingEnemy)
EndFunction

Bool Function Owns(Actor subject)
    Return armed && subject != None && subject.GetBaseObject() == expectedBase && subject.GetValue(IdentityStamp) == stamp
EndFunction

Function Inspect()
    Actor player = Game.GetPlayer()
    Actor subject = Hunt.Nemesis.GetActorReference()
    If !Check("same-existing-enemy", Owns(subject))
        Return
    EndIf
    Check("alive", !subject.IsDead())
    Check("loaded-at-player", subject.Is3DLoaded() && subject.GetParentCell() == player.GetParentCell())
    Check("original-planet", player.GetCurrentPlanet() == originPlanet)
    Check("buff-and-debuff", subject.HasSpell(Hunt.NemesisBuff) && player.HasSpell(Hunt.PlayerDebuff))
    Check("hunt-objective", Hunt.IsObjectiveDisplayed(10))
    Check("controls-restored", !OSFDefeat.IsDefeated(player) && Game.IsMovementControlsEnabled() && Game.IsFightingControlsEnabled())
    Check("ai-not-suspended", !OSFDefeat.IsActorAISuspended(subject))
    ObjectReference item = Hunt.StolenWeapon.GetReference()
    If item != None
        Check("stolen-weapon-owned", subject.GetItemCount(item.GetBaseObject()) > 0)
    EndIf
    Log("INSPECT|initial-id=" + initialID + "|current-id=" + subject.GetFormID() + "|cell=" + subject.GetParentCell().GetFormID() + "|theft-status=" + Hunt.LastTheftStatus + "|hours=" + ((Utility.GetCurrentGameTime() - beganDay) * 24.0) + "|reset-events=" + resetEvents)
    Log("VISUAL-REQUIRED|normal full-body appearance, visible title, journal objective and abilities")
EndFunction

Function Away()
    Actor subject = Hunt.Nemesis.GetActorReference()
    Bool ok = Check("away-identity", Owns(subject))
    If !ok
        Return
    EndIf
    ok = Check("away-other-planet", Game.GetPlayer().GetCurrentPlanet() != None && Game.GetPlayer().GetCurrentPlanet() != originPlanet) && ok
    ok = Check("away-unloaded", !subject.Is3DLoaded() && detached) && ok
    If ok
        observedAway = True
        awayDay = Utility.GetCurrentGameTime()
        Log("AWAY|day=" + awayDay + "|save and reload here before returning normally")
    EndIf
EndFunction

Function Returned()
    If !observedAway
        Log("BLOCKED|No observed departure; inspect is not travel evidence.")
        Return
    EndIf
    Actor subject = Hunt.Nemesis.GetActorReference()
    Bool ok = Owns(subject) && Game.GetPlayer().GetCurrentPlanet() == originPlanet
    If subject != None
        ok = ok && subject.Is3DLoaded() && subject.GetParentCell() == Game.GetPlayer().GetParentCell()
    EndIf
    If Check("normal-return", ok)
        trips += 1
        observedAway = False
        Log("RETURN|trip=" + trips + "|hours-away=" + ((Utility.GetCurrentGameTime() - awayDay) * 24.0))
    EndIf
    Inspect()
EndFunction

Event ReferenceAlias.OnCellDetach(ReferenceAlias sender)
    If sender == Hunt.Nemesis
        detached = True
        Log("DETACH|nemesis")
    EndIf
EndEvent

Event ReferenceAlias.OnCellAttach(ReferenceAlias sender)
    If sender == Hunt.Nemesis
        detached = False
        Log("ATTACH|nemesis")
    EndIf
EndEvent

Event ReferenceAlias.OnReset(ReferenceAlias sender)
    If sender == Hunt.Nemesis
        resetEvents += 1
        Log("RESET|nemesis alias received OnReset; inspect identity and loot")
    EndIf
EndEvent

Function Cleanup()
    Hunt.CancelHunt()
    OSFDefeat.ForceRecoverAll()
    Log("CLEANUP|Discard the private saves. Existing enemy and stolen possessions are not deleted.")
    Stop()
EndFunction
