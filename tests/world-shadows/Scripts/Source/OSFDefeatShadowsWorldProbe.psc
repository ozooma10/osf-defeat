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
Int defeatToken = 0
; Same-process death diagnostics keep a scalar ID and base form only. The
; production alias remains the only reference persistence mechanism.
Int deathSubjectID = 0
Int deathEventKillerID = 0
Bool deathEventObserved = False
Weapon deathWeaponBase

; Resolve through the plugin filename so native-console setup does not depend
; on EditorID lookup or an assumed load-order prefix.
Function StartProbe() Global
    Debug.OpenUserLog("OSFDefeatShadowsWorld")
    OSFDefeatShadowsWorldProbe probe = Game.GetFormFromFile(0x000900, "OSFDefeatShadows.esm") as OSFDefeatShadowsWorldProbe
    If probe == None
        Debug.TraceUser("OSFDefeatShadowsWorld", "BLOCKED|World probe form or script unavailable.")
        Return
    EndIf
    If !probe.IsRunning()
        probe.Start()
    EndIf
    Debug.TraceUser("OSFDefeatShadowsWorld", "PROBE|id=" + probe.GetFormID() + "|running=" + probe.IsRunning())
    probe.ListCombatTargets()
    probe.WatchDeath()
EndFunction

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

; Observe existing combatants through Papyrus so setup can use the harness's
; native command channel without opening the console or creating an actor.
Function ListCombatTargets()
    Actor player = Game.GetPlayer()
    Actor[] targets = player.GetAllCombatTargets()
    Int i = 0
    Log("CANDIDATES|count=" + targets.Length)
    While i < targets.Length
        Actor target = targets[i]
        If target != None
            Log("CANDIDATE|id=" + target.GetFormID() + "|base=" + target.GetBaseObject().GetFormID() + "|eligible=" + Hunt.Eligible(target) + "|loaded=" + target.Is3DLoaded() + "|distance=" + player.GetDistance(target))
            ActorBase base = target.GetLeveledActorBase()
            Log("GATES|id=" + target.GetFormID() + "|dead=" + target.IsDead() + "|essential=" + target.IsEssential() + "|teammate=" + target.IsPlayerTeammate() + "|commanded=" + target.IsCommandedActor() + "|child=" + target.IsChild() + "|ghost=" + target.IsGhost())
            If base != None
                Log("BASE-GATES|id=" + target.GetFormID() + "|leveled-base=" + base.GetFormID() + "|protected=" + base.IsProtected() + "|essential=" + base.IsEssential() + "|unique=" + base.IsUnique())
            EndIf
            Log("WORLD-GATES|id=" + target.GetFormID() + "|quest-alias=" + OSFDefeat.HasQuestAlias(target) + "|protection-override=" + OSFDefeat.HasProtectionOverride(target) + "|scene=" + target.IsInScene() + "|level=" + target.GetLevel() + "|player-level=" + player.GetLevel() + "|level-gap=" + Hunt.LevelGap)
        EndIf
        i += 1
    EndWhile
EndFunction

Function BeginTrial(String kind)
    If armed || (kind != "authored" && kind != "generated")
        Log("BLOCKED|Provide authored/generated kind, once per clean seed.")
        Return
    EndIf
    If !Hunt.IsRunning()
        Hunt.Start()
    EndIf
    Actor player = Game.GetPlayer()
    If Hunt.Nemesis.GetReference() != None
        Log("BLOCKED|The hunt slot must be empty.")
        Return
    EndIf
    If player.GetCurrentPlanet() == None || player.IsInSpaceEx(True) || player.GetCurrentShipRef() != None
        Log("BLOCKED|Begin at a planetary combat site, outside ships.")
        Return
    EndIf
    siteKind = kind
    originPlanet = player.GetCurrentPlanet()
    armed = True
    defeatToken = OSFDefeat.RegisterForActorDefeated(Self, "OnTrialDefeat")
    Log("ARMED|await natural lethal attack at this existing-enemy site|callback=" + defeatToken)
EndFunction

Function OnTrialDefeat(Actor victim, Actor existingEnemy)
    Actor player = Game.GetPlayer()
    If !armed || stamp != 0 || victim != player
        Return
    EndIf
    OSFDefeat.UnregisterForActorDefeated(defeatToken)
    defeatToken = 0
    ; This callback runs at down-state entry, before the aftermath's two-second
    ; delay and promotion. Observe the actual existing aggressor rather than
    ; guessing which combatant will deliver the lethal hit. Never retain Actor.
    If existingEnemy == None || !Hunt.Eligible(existingEnemy) || existingEnemy.GetParentCell() != player.GetParentCell() || !existingEnemy.Is3DLoaded() || player.GetCurrentPlanet() != originPlanet || OSFDefeat.GetDefeatCause(player) != 1
        Log("BLOCKED|Actual lethal aggressor must be eligible and loaded at the original planetary site.")
        Return
    EndIf
    stamp = Utility.RandomInt(100000, 16000000)
    expectedBase = existingEnemy.GetBaseObject()
    initialID = existingEnemy.GetFormID()
    beganDay = Utility.GetCurrentGameTime()
    existingEnemy.SetValue(IdentityStamp, stamp)
    RegisterForRemoteEvent(Hunt.Nemesis, "OnCellDetach")
    RegisterForRemoteEvent(Hunt.Nemesis, "OnCellAttach")
    RegisterForRemoteEvent(Hunt.Nemesis, "OnReset")
    Log("BEGIN|natural lethal attack from existing enemy=" + initialID + "|base=" + expectedBase.GetFormID() + "|stamp=" + stamp + "|planet=" + originPlanet.GetFormID() + "|day=" + beganDay)
    RegisterForRemoteEvent(Hunt.Nemesis, "OnDying")
    RegisterForRemoteEvent(Hunt.Nemesis, "OnDeath")
EndFunction

Bool Function Owns(Actor subject)
    Return armed && stamp != 0 && subject != None && subject.GetBaseObject() == expectedBase && subject.GetValue(IdentityStamp) == stamp
EndFunction

Bool Function LoadedNearby(Actor subject)
    ; The generated fixture reports different parent cells after promotion even
    ; with the same loaded robot visible 25-36 units away. Cell identity is not
    ; a proximity oracle. Keep a bounded distance and require the original planet;
    ; visual/site-route evidence is still a separate acceptance requirement.
    Actor player = Game.GetPlayer()
    Return subject != None && subject.Is3DLoaded() && player.GetCurrentPlanet() == originPlanet && player.GetDistance(subject) <= 100.0
EndFunction

Function WatchDeath()
    Actor subject = Hunt.Nemesis.GetActorReference()
    If !Owns(subject) || subject.IsDead()
        Log("BLOCKED|Death observation requires the original living hunt.")
        Return
    EndIf
    deathSubjectID = subject.GetFormID()
    deathWeaponBase = Hunt.StolenWeaponBase
    deathEventObserved = False
    deathEventKillerID = 0
    RegisterForRemoteEvent(Hunt.Nemesis, "OnDying")
    RegisterForRemoteEvent(Hunt.Nemesis, "OnDeath")
    Log("WATCH-DEATH|subject=" + deathSubjectID + "|level=" + subject.GetLevel() + "|player-level=" + Game.GetPlayer().GetLevel())
EndFunction

Function ObserveResolution(Bool retrieved = False)
    ; Runtime IDs may remap on load. Re-arm WatchDeath before combat after each
    ; load; reject a missing/mismatched ID instead of finding or retaining a ref.
    Actor subject = Game.GetForm(deathSubjectID) as Actor
    If !Owns(subject)
        Log("BLOCKED|Death subject ID unavailable or identity changed; no resolution claim.")
        Return
    EndIf
    Actor player = Game.GetPlayer()
    Actor killer = subject.GetKiller()
    Int killerID = 0
    If killer != None
        killerID = killer.GetFormID()
    EndIf
    Log("DEATH-ATTRIBUTION|subject=" + deathSubjectID + "|dead=" + subject.IsDead() + "|event-observed=" + deathEventObserved + "|event-killer=" + deathEventKillerID + "|actor-killer=" + killerID + "|level=" + subject.GetLevel() + "|player-level=" + player.GetLevel())
    If !Check("natural-death-observed", subject.IsDead() && deathEventObserved)
        Return
    EndIf
    Actor currentNemesis = Hunt.Nemesis.GetActorReference()
    If currentNemesis != None && currentNemesis != subject
        ; Repeated combat can promote another hunt before the corpse is looted.
        ; Its shared quest/player state cannot certify the original cleanup.
        Log("BLOCKED|New hunt occupies the slot; use the earlier resolution observation for cleanup/reward. Original corpse loot remains independently observable.")
    Else
        Check("death-hunt-cleared", currentNemesis == None && Hunt.StolenWeaponBase == None)
        Check("death-abilities-cleared", !subject.HasSpell(Hunt.NemesisBuff) && !player.HasSpell(Hunt.PlayerDebuff))
        Check("death-objective-complete", Hunt.IsObjectiveCompleted(10))
        Check("death-reward-matches-engine-killer", player.HasSpell(Hunt.PlayerReward) == (killer == player && subject.GetLevel() >= player.GetLevel()))
    EndIf
    If deathWeaponBase != None
        Log("DEATH-LOOT|weapon=" + deathWeaponBase.GetFormID() + "|player-weapons=" + player.GetItemCount(deathWeaponBase) + "|corpse-weapons=" + subject.GetItemCount(deathWeaponBase) + "|player-credits=" + player.GetItemCount(Game.GetCredits()) + "|corpse-credits=" + subject.GetItemCount(Game.GetCredits()))
        If retrieved
            Check("stolen-weapon-retrieved", player.GetItemCount(deathWeaponBase) == 1 && subject.GetItemCount(deathWeaponBase) == 0)
        EndIf
    EndIf
    Log("DEATH-POSITION|x=" + subject.GetPositionX() + "|y=" + subject.GetPositionY() + "|z=" + subject.GetPositionZ())
EndFunction

Event ReferenceAlias.OnDying(ReferenceAlias sender, ObjectReference killer)
    If sender == Hunt.Nemesis
        Int killerID = 0
        If killer != None
            killerID = killer.GetFormID()
        EndIf
        Log("DYING-EVENT|killer=" + killerID)
    EndIf
EndEvent

Event ReferenceAlias.OnDeath(ReferenceAlias sender, ObjectReference killer)
    If sender == Hunt.Nemesis
        deathEventObserved = True
        deathEventKillerID = 0
        If killer != None
            deathEventKillerID = killer.GetFormID()
        EndIf
        Log("DEATH-EVENT|killer=" + deathEventKillerID)
        ; Do not let a later hunt overwrite the observed death attribution.
        UnregisterForRemoteEvent(Hunt.Nemesis, "OnDying")
        UnregisterForRemoteEvent(Hunt.Nemesis, "OnDeath")
    EndIf
EndEvent

Function Inspect()
    Actor player = Game.GetPlayer()
    Actor subject = Hunt.Nemesis.GetActorReference()
    If !Check("same-existing-enemy", Owns(subject))
        Return
    EndIf
    Check("alive", !subject.IsDead())
    Log("SPATIAL|loaded=" + subject.Is3DLoaded() + "|distance=" + player.GetDistance(subject) + "|player-cell=" + player.GetParentCell().GetFormID() + "|subject-cell=" + subject.GetParentCell().GetFormID() + "|subject-x=" + subject.GetPositionX() + "|subject-y=" + subject.GetPositionY() + "|subject-z=" + subject.GetPositionZ())
    Check("loaded-near-player", LoadedNearby(subject))
    Check("original-planet", player.GetCurrentPlanet() == originPlanet)
    Check("buff-and-debuff", subject.HasSpell(Hunt.NemesisBuff) && player.HasSpell(Hunt.PlayerDebuff))
    Check("hunt-objective", Hunt.IsObjectiveDisplayed(10))
    Check("controls-restored", !OSFDefeat.IsDefeated(player) && Game.IsMovementControlsEnabled() && Game.IsFightingControlsEnabled())
    Check("ai-not-suspended", !OSFDefeat.IsActorAISuspended(subject))
    Weapon item = Hunt.StolenWeaponBase
    If item != None
        Check("stolen-weapon-owned", subject.GetItemCount(item) > 0 && player.GetItemCount(item) == 0)
        Log("LOOT|weapon=" + item.GetFormID() + "|player-weapons=" + player.GetItemCount(item) + "|nemesis-weapons=" + subject.GetItemCount(item) + "|player-credits=" + player.GetItemCount(Game.GetCredits()) + "|nemesis-credits=" + subject.GetItemCount(Game.GetCredits()))
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
        If !observedAway
            awayDay = Utility.GetCurrentGameTime()
        EndIf
        observedAway = True
        Log("AWAY|day=" + awayDay + "|hours-away=" + ((Utility.GetCurrentGameTime() - awayDay) * 24.0) + "|save and reload here before returning normally")
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
        ok = ok && LoadedNearby(subject)
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
    If defeatToken != 0
        OSFDefeat.UnregisterForActorDefeated(defeatToken)
        defeatToken = 0
    EndIf
    Hunt.CancelHunt()
    OSFDefeat.ForceRecoverAll()
    Log("CLEANUP|Discard the private saves. Existing enemy and stolen possessions are not deleted.")
    Stop()
EndFunction
