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

Bool busy = False
Bool deathPending = False
ObjectReference pendingKiller
Int deadObservations = 0

Event OnQuestInit()
    WatchPlayer()
    StartTimer(5.0)
EndEvent

Function WatchPlayer()
    Actor player = Game.GetPlayer()
    RegisterForRemoteEvent(player, "OnPlayerLoadGame")
    RegisterForRemoteEvent(player, "OnPlayerFireWeapon")
    ; Starfield graph events, not Skyrim's weaponSwing names. HitFrame is the
    ; melee contact frame even when there is no target; health damage is also
    ; handled by the native fallback. Missed attacks still need live acceptance.
    RegisterForAnimationEvent(player, "HitFrame")
    RegisterForAnimationEvent(player, "MeleeAttackStart")
    RegisterForAnimationEvent(player, "MeleeAttackBashStart")
    RegisterForAnimationEvent(player, "ThrowGrenade")
    RegisterForAnimationEvent(player, "ThrowMine")
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
    If busy || Nemesis.GetReference() != None
        Return False
    EndIf
    busy = True
    If !Eligible(candidate)
        busy = False
        Return False
    EndIf
    Nemesis.ForceRefTo(candidate)
    If Nemesis.GetActorReference() != candidate
        busy = False
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
    busy = False
    If deathPending || candidate.IsDead()
        ResolveDeath(pendingKiller)
    EndIf
    Return True
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
    ; AddItem(reference) uses the engine's reference-transfer path (also used by
    ; vanilla TestSteveCQuestScript), preserving this instance and its alias.
    ; RemoveItem(reference, ..., destination) can refuse the newly aliased item.
    If item.GetContainer() != player
        LastTheftStatus = 6
        StolenWeapon.Clear()
        Return
    EndIf
    candidate.AddItem(item, 1, True)
    Int attempts = 0
    While item.GetContainer() != candidate && attempts < 10
        Utility.Wait(0.1)
        attempts += 1
    EndWhile
    If item.GetContainer() == candidate && player.GetItemCount(weaponBase) == 0
        LastTheftStatus = 7
        candidate.EquipItem(item, False, True)
    Else
        LastTheftStatus = 6
        StolenWeapon.Clear()
    EndIf
EndFunction

Function ResolveDeath(ObjectReference killer)
    If busy
        deathPending = True
        pendingKiller = killer
        Return
    EndIf
    Actor target = Nemesis.GetActorReference()
    If target == None || !target.IsDead()
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
    deathPending = False
    pendingKiller = None
    busy = False
EndFunction

; Explicit pack cleanup before disabling it. Loot remains where it was transferred.
Function CancelHunt()
    If busy
        Return
    EndIf
    busy = True
    Actor target = Nemesis.GetActorReference()
    If target != None
        target.RemoveSpell(NemesisBuff)
        UnregisterForRemoteEvent(target, "OnDeath")
    EndIf
    Game.GetPlayer().RemoveSpell(PlayerDebuff)
    Game.GetPlayer().RemoveSpell(PlayerReward)
    OSFDefeat.EndRecoveryTruce(Game.GetPlayer())
    SetObjectiveDisplayed(10, False)
    StolenWeapon.Clear()
    Nemesis.Clear()
    deathPending = False
    pendingKiller = None
    busy = False
EndFunction

Event Actor.OnDeath(Actor sender, ObjectReference killer)
    If sender == Nemesis.GetActorReference()
        ResolveDeath(killer)
    EndIf
EndEvent

Event Actor.OnPlayerFireWeapon(Actor sender, Form baseObject)
    OSFDefeat.EndRecoveryTruce(sender)
EndEvent

Event OnAnimationEvent(ObjectReference source, String eventName)
    If source == Game.GetPlayer()
        OSFDefeat.EndRecoveryTruce(Game.GetPlayer())
    EndIf
EndEvent

Event Actor.OnPlayerLoadGame(Actor sender)
    WatchPlayer()
    StartTimer(5.0)
EndEvent

Event OnTimer(Int timerID)
    If !busy
        Actor target = Nemesis.GetActorReference()
        If target != None
            If target.IsDead()
                ; Allow the VM to deliver OnDeath before the fallback claims this
                ; transition. A timer racing the death event must not steal a reward.
                deadObservations += 1
                If deadObservations >= 4
                    ResolveDeath(None)
                EndIf
            Else
                deadObservations = 0
                If !Game.GetPlayer().HasSpell(PlayerDebuff)
                    Game.GetPlayer().AddSpell(PlayerDebuff, False)
                EndIf
            EndIf
        EndIf
        If OSFDefeat.IsRecoveryTruceActive(Game.GetPlayer())
            WatchPlayer()
        EndIf
    EndIf
    StartTimer(5.0)
EndEvent
