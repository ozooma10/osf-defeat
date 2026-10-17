Scriptname OSFDefeatShadowsHunt extends Quest

ReferenceAlias Property Nemesis Auto Const Mandatory
ReferenceAlias Property StolenWeapon Auto Const Mandatory
Spell Property NemesisBuff Auto Const Mandatory
Spell Property PlayerDebuff Auto Const Mandatory
Spell Property PlayerReward Auto Const Mandatory
Int Property LevelGap = 15 Auto Const
; Observable aftermath result: 1 unarmed, 2 ambiguous stack, 3 not better,
; 4 alias failed, 5 quest item, 6 transfer refused, 7 transferred.
Int Property LastTheftStatus = 0 Auto Hidden
Weapon Property LastWeaponConsidered Auto Hidden
String Property LastAttackEvent Auto Hidden

Bool busy = False
Bool deathPending = False
ObjectReference pendingKiller
Int deadObservations = 0
; Quest.Stop clears aliases BEFORE OnQuestShutdown. Keep the actor whose
; ability we own independently, including across saves.
Actor ownedTarget
Bool cleanupPending = False
Bool removeRewardPending = False
Bool shuttingDown = False

Event OnQuestInit()
    shuttingDown = False
    WatchPlayer()
    StartTimer(5.0)
EndEvent

Event OnQuestStarted()
    shuttingDown = False
    WatchPlayer()
    StartTimer(5.0)
EndEvent

Event OnQuestShutdown()
    shuttingDown = True
    CancelTimer()
    UnregisterForAllEvents()
    CancelHunt()
EndEvent

Function WatchPlayer()
    If shuttingDown || !IsRunning()
        Return
    EndIf
    Actor player = Game.GetPlayer()
    RegisterForRemoteEvent(player, "OnPlayerLoadGame")
    RegisterForRemoteEvent(player, "OnPlayerFireWeapon")
    RegisterForRemoteEvent(player, "OnSpellCast")
    ; .244 emits WeaponFire when a grenade is released; ThrowGrenade is an
    ; input/clip name, not the outgoing event. HitFrame covers missed melee.
    ; Keep the player-fire event and native damage path as additional coverage.
    RegisterForAnimationEvent(player, "WeaponFire")
    RegisterForAnimationEvent(player, "HitFrame")
    RegisterForAnimationEvent(player, "MeleeAttackStart")
    RegisterForAnimationEvent(player, "MeleeAttackBashStart")
EndFunction

Bool Function Eligible(Actor candidate)
    Actor player = Game.GetPlayer()
    If candidate == None || candidate == player || candidate.IsDead()
        Return False
    EndIf
    If candidate.IsEssential() || candidate.IsPlayerTeammate() || candidate.IsCommandedActor() || candidate.IsChild() || candidate.IsGhost()
        Return False
    EndIf
    ActorBase base = candidate.GetLeveledActorBase()
    If base == None || base.IsProtected() || base.IsEssential() || base.IsUnique()
        Return False
    EndIf
    If OSFDefeat.HasQuestAlias(candidate) || OSFDefeat.HasProtectionOverride(candidate) || candidate.IsInScene()
        Return False
    EndIf
    Return candidate.GetLevel() <= player.GetLevel() + LevelGap
EndFunction

Bool Function Promote(Actor candidate, Weapon lostWeapon = None)
    If busy || cleanupPending || shuttingDown || !IsRunning() || ownedTarget != None || Nemesis.GetReference() != None
        Return False
    EndIf
    busy = True
    If !Eligible(candidate)
        FinishMutation()
        Return False
    EndIf
    ownedTarget = candidate
    Nemesis.ForceRefTo(candidate)
    If Nemesis.GetActorReference() != candidate || !IsRunning() || cleanupPending
        RequestCleanup(False)
        FinishMutation()
        Return False
    EndIf
    deathPending = False
    pendingKiller = None
    deadObservations = 0
    RegisterForRemoteEvent(candidate, "OnDeath")
    candidate.AddSpell(NemesisBuff, False)
    Game.GetPlayer().AddSpell(PlayerDebuff, True)
    SetObjectiveCompleted(10, False)
    SetObjectiveFailed(10, False)
    SetObjectiveDisplayed(10, True, True)
    TakeLoot(candidate, lostWeapon)
    Bool accepted = !cleanupPending && IsRunning()
    FinishMutation()
    If accepted && (deathPending || candidate.IsDead())
        ResolveDeath(pendingKiller, candidate)
    EndIf
    Return accepted
EndFunction

Function TakeLoot(Actor candidate, Weapon weaponBase)
    Actor player = Game.GetPlayer()
    MiscObject credits = Game.GetCredits()
    Int available = player.GetItemCount(credits)
    Int taken = (available * 0.10) as Int
    If taken < 25
        taken = 25
    ElseIf taken > 2500
        taken = 2500
    EndIf
    If taken > available
        taken = available
    EndIf
    If taken > 0
        player.RemoveItem(credits, taken, True, candidate)
    EndIf

    Weapon theirs = candidate.GetEquippedWeapon()
    LastWeaponConsidered = weaponBase
    ; v1 quality proxy: base credit value. No stat/instance API is claimed here.
    ; A single copy is required because GetEquippedWeapon returns only a base.
    If weaponBase == None
        LastTheftStatus = 1
        Return
    EndIf
    If player.GetItemCount(weaponBase) != 1
        LastTheftStatus = 2
        Return
    EndIf
    ; NPC getters can retain an inherent or no-longer-owned weapon base.
    If theirs != None && candidate.GetItemCount(theirs) > 0 && theirs.GetGoldValue() >= weaponBase.GetGoldValue()
        LastTheftStatus = 3
        Return
    EndIf
    ObjectReference item = player.MakeAliasedRefFromInventory(weaponBase, StolenWeapon)
    If item == None
        LastTheftStatus = 4
        Return
    EndIf
    If item.IsQuestItem()
        LastTheftStatus = 5
        StolenWeapon.Clear()
        Return
    EndIf
    ; The aliased inventory proxy can report GetContainer=None. Transfer the
    ; sole inventory instance by base instead; this moves it, never recreates it.
    ; The single-copy guard above makes the choice unambiguous.
    Int beforeCount = candidate.GetItemCount(weaponBase)
    Int moved = player.RemoveItem(weaponBase, 1, True, candidate)
    If moved == 1 && player.GetItemCount(weaponBase) == 0 && candidate.GetItemCount(weaponBase) == beforeCount + 1
        LastTheftStatus = 7
        ; The AI can immediately switch to fists if the stolen gun uses an
        ; ammunition type absent from its original loadout. Supply one round
        ; only when needed; do not take more of the player's inventory.
        Ammo ammunition = weaponBase.GetAmmo()
        If ammunition != None && candidate.GetItemCount(ammunition) == 0
            candidate.AddItem(ammunition, 1, True)
        EndIf
        candidate.EquipItem(weaponBase, False, True)
    Else
        LastTheftStatus = 6
        StolenWeapon.Clear()
    EndIf
EndFunction

Function ResolveDeath(ObjectReference killer, Actor dying = None)
    Actor target = Nemesis.GetActorReference()
    If cleanupPending || target == None || (dying != None && dying != target) || !target.IsDead()
        Return
    EndIf
    If busy
        deathPending = True
        pendingKiller = killer
        Return
    EndIf
    busy = True
    Actor player = Game.GetPlayer()
    player.RemoveSpell(PlayerDebuff)
    If killer == player && target.GetLevel() >= player.GetLevel()
        player.AddSpell(PlayerReward, True)
        Debug.Notification("Nemesis defeated. You have earned Hardened Resolve.")
    Else
        Debug.Notification("Your nemesis is dead. Your wound has lifted.")
    EndIf
    target.RemoveSpell(NemesisBuff)
    UnregisterForRemoteEvent(target, "OnDeath")
    SetObjectiveCompleted(10, True)
    StolenWeapon.Clear()
    Nemesis.Clear()
    ownedTarget = None
    deathPending = False
    pendingKiller = None
    deadObservations = 0
    FinishMutation()
EndFunction

; Explicit pack cleanup before disabling it. Loot remains where it was transferred.
Function CancelHunt()
    RequestCleanup(True)
EndFunction

Function RequestCleanup(Bool removeReward)
    ; A request during promotion/death must survive until that mutation finishes.
    ; Keeping busy set prevents a new promotion from racing the queued cleanup.
    cleanupPending = True
    If removeReward
        removeRewardPending = True
    EndIf
    If busy
        Return
    EndIf
    busy = True
    FinishMutation()
EndFunction

Function FinishMutation()
    While cleanupPending
        cleanupPending = False
        Bool removeReward = removeRewardPending
        removeRewardPending = False
        ClearOwnedHunt(removeReward)
    EndWhile
    busy = False
EndFunction

Function ClearOwnedHunt(Bool removeReward)
    Actor target = ownedTarget
    If target == None
        ; Saves made before ownedTarget existed still have their live alias.
        target = Nemesis.GetActorReference()
    EndIf
    If target != None
        target.RemoveSpell(NemesisBuff)
        UnregisterForRemoteEvent(target, "OnDeath")
    EndIf
    Game.GetPlayer().RemoveSpell(PlayerDebuff)
    If removeReward
        Game.GetPlayer().RemoveSpell(PlayerReward)
        OSFDefeat.EndRecoveryTruce(Game.GetPlayer())
    EndIf
    ; Shutdown can fail an objective, and a previous win can complete it.
    ; Return it to an unfinished state before hiding the reusable hunt entry.
    SetObjectiveCompleted(10, False)
    SetObjectiveFailed(10, False)
    SetObjectiveDisplayed(10, False)
    StolenWeapon.Clear()
    Nemesis.Clear()
    ownedTarget = None
    deathPending = False
    pendingKiller = None
    deadObservations = 0
EndFunction

Event Actor.OnDeath(Actor sender, ObjectReference killer)
    If sender == Nemesis.GetActorReference()
        ResolveDeath(killer, sender)
    EndIf
EndEvent

Event Actor.OnPlayerFireWeapon(Actor sender, Form baseObject)
    If shuttingDown || !IsRunning()
        Return
    EndIf
    LastAttackEvent = "OnPlayerFireWeapon"
    OSFDefeat.EndRecoveryTruce(sender)
EndEvent

Event ObjectReference.OnSpellCast(ObjectReference sender, Form spellCast)
    Spell attack = spellCast as Spell
    If !shuttingDown && IsRunning() && sender == Game.GetPlayer() && attack != None && attack.IsHostile()
        LastAttackEvent = "OnSpellCast"
        OSFDefeat.EndRecoveryTruce(Game.GetPlayer())
    EndIf
EndEvent

Event OnAnimationEvent(ObjectReference source, String eventName)
    If !shuttingDown && IsRunning() && source == Game.GetPlayer()
        LastAttackEvent = eventName
        OSFDefeat.EndRecoveryTruce(Game.GetPlayer())
    EndIf
EndEvent

Event Actor.OnPlayerLoadGame(Actor sender)
    If !IsRunning()
        shuttingDown = True
        CancelTimer()
        UnregisterForAllEvents()
        CancelHunt()
        Return
    EndIf
    ; Adopt the alias when upgrading a save from the original hunt script.
    If ownedTarget == None
        ownedTarget = Nemesis.GetActorReference()
    EndIf
    WatchPlayer()
    StartTimer(5.0)
EndEvent

Event OnTimer(Int timerID)
    If shuttingDown || !IsRunning()
        Return
    EndIf
    If !busy
        busy = True
        Actor target = Nemesis.GetActorReference()
        If ownedTarget == None
            ownedTarget = target
        EndIf
        If target != ownedTarget || (target == None && (Game.GetPlayer().HasSpell(PlayerDebuff) || StolenWeapon.GetReference() != None))
            ; Alias loss is an abandoned hunt, not a credited kill. Preserve a
            ; previously earned reward and the recovery truce, but lift the wound.
            RequestCleanup(False)
        ElseIf target != None
            If target.IsDead()
                ; Allow the VM to deliver OnDeath before the fallback claims this
                ; transition. A timer racing the death event must not steal a reward.
                deadObservations += 1
                If deadObservations >= 4 && !deathPending
                    deathPending = True
                    pendingKiller = None
                EndIf
            Else
                deadObservations = 0
                If !Game.GetPlayer().HasSpell(PlayerDebuff)
                    Game.GetPlayer().AddSpell(PlayerDebuff, False)
                EndIf
            EndIf
        EndIf
        FinishMutation()
        If deathPending
            ResolveDeath(pendingKiller, target)
        EndIf
        If OSFDefeat.IsRecoveryTruceActive(Game.GetPlayer())
            WatchPlayer()
        EndIf
    EndIf
    If !shuttingDown && IsRunning()
        StartTimer(5.0)
    EndIf
EndEvent
