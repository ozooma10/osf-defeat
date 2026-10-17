Scriptname OSFDefeatShadowsHunt extends Quest

ReferenceAlias Property Nemesis Auto Const Mandatory
ReferenceAlias Property StolenWeapon Auto Const Mandatory
Spell Property NemesisBuff Auto Const Mandatory
Spell Property PlayerDebuff Auto Const Mandatory
Spell Property PlayerReward Auto Const Mandatory
Int Property LevelGap = 15 Auto Const

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

Bool Function Promote(Actor candidate)
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
    TakeLoot(candidate)
    busy = False
    If deathPending || candidate.IsDead()
        ResolveDeath(pendingKiller)
    EndIf
    Return True
EndFunction

Function TakeLoot(Actor candidate)
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

    Weapon weaponBase = player.GetEquippedWeapon()
    Weapon theirs = candidate.GetEquippedWeapon()
    ; v1 quality proxy: base credit value. No stat/instance API is claimed here.
    ; A single copy is required because GetEquippedWeapon returns only a base.
    If weaponBase == None || player.GetItemCount(weaponBase) != 1
        Return
    EndIf
    If theirs != None && theirs.GetGoldValue() >= weaponBase.GetGoldValue()
        Return
    EndIf
    ObjectReference item = player.MakeAliasedRefFromInventory(weaponBase, StolenWeapon)
    If item == None
        Return
    EndIf
    If item.IsQuestItem()
        StolenWeapon.Clear()
        Return
    EndIf
    ; Transfer the reference, preserving its instance data, never recreate its base.
    Int moved = player.RemoveItem(item, 1, True, candidate)
    If moved == 1
        candidate.EquipItem(item, False, True)
    Else
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
