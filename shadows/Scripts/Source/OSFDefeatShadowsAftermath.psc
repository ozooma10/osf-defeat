Scriptname OSFDefeatShadowsAftermath extends Quest

OSFDefeatShadowsHunt Property Hunt Auto Const Mandatory
Int Property BlackoutHours = 2 Auto Const
Float Property TruceSeconds = 30.0 Auto Const
Int Property Phase = 0 Auto Hidden
String Property LastResult Auto Hidden
Int generation = 0
Bool active = False

Event OnQuestInit()
    RunAftermath()
EndEvent

Function RunAftermath()
    generation += 1
    Int run = generation
    active = True
    Phase = 0
    LastResult = "running"
    Actor player = Game.GetPlayer()
    Actor aggressor = OSFDefeat.GetLastPlayerAggressor()
    Weapon lostWeapon = OSFDefeat.GetLastPlayerEquippedWeapon()
    If !OSFDefeat.IsDefeated(player)
        active = False
        LastResult = "not-defeated"
        Stop()
        Return
    EndIf
    ; The on-foot domain describes the victim, not their cell. Refuse blackout
    ; in ship interiors or space, where a future return is not yet established.
    If player.IsInSpaceEx(True) || player.GetCurrentShipRef() != None
        OSFDefeat.RecoverActor(player)
        Int retries = 0
        While IsCurrent(run) && OSFDefeat.IsDefeated(player) && retries < 50
            Utility.Wait(0.1)
            retries += 1
        EndWhile
        If !IsCurrent(run)
            Return
        EndIf
        If !OSFDefeat.IsDefeated(player)
            OSFDefeat.ResistDefeat(player, 30.0)
            Utility.Wait(0.1)
            If IsCurrent(run)
                player.Kill(aggressor)
            EndIf
        EndIf
        active = False
        LastResult = "unsupported-domain"
        Stop()
        Return
    EndIf
    If !Hunt.IsRunning()
        Hunt.Start()
    EndIf
    If !IsCurrent(run)
        Return
    EndIf
    ; PassTime advances the framework's game-time watchdog too. The registration
    ; leases 12,000 seconds (over three game hours even at time scale 1) and a real
    ; Papyrus timer below recovers/fades in if any latent step gets stuck.
    RegisterForRemoteEvent(player, "OnPlayerLoadGame")
    StartTimer(30.0, 1)
    Phase = 1
    Game.FadeOutGame(True, True, 0.0, 0.6, True)
    Utility.Wait(2.0)
    If !IsCurrent(run)
        Return
    EndIf
    Bool promoted = Hunt.Promote(aggressor, lostWeapon)
    If !IsCurrent(run)
        Return
    EndIf
    Phase = 2
    Float wakeDay = Utility.GetCurrentGameTime() + (BlackoutHours / 24.0)
    Game.PassTime(BlackoutHours)
    If !IsCurrent(run)
        Return
    EndIf
    ; PassTime lands on an hour boundary and can advance less than requested.
    ; One additional hour ensures at least the requested elapsed time.
    If Utility.GetCurrentGameTime() < wakeDay
        Game.PassTime(1)
    EndIf
    If !IsCurrent(run)
        Return
    EndIf
    Phase = 3
    OSFDefeat.RecoverActorWithTruce(player, TruceSeconds)
    Int tries = 0
    While IsCurrent(run) && OSFDefeat.IsDefeated(player) && tries < 50
        Utility.Wait(0.1)
        tries += 1
    EndWhile
    If !IsCurrent(run)
        Return
    EndIf
    If OSFDefeat.IsDefeated(player)
        ; Never strand the player behind a fade if the native request was refused.
        OSFDefeat.RecoverActor(player)
    EndIf
    Game.FadeOutGame(False, True, 0.0, 0.8)
    CancelTimer(1)
    UnregisterForRemoteEvent(player, "OnPlayerLoadGame")
    active = False
    Phase = 4
    LastResult = "completed"
    If promoted
        Debug.Notification("Hours later. Your attacker left a wound and a trail. Check your journal.")
    Else
        Debug.Notification("Hours later. You have a brief chance to get away.")
    EndIf
    Stop()
EndFunction

Bool Function IsCurrent(Int run)
    Return active && generation == run && IsRunning()
EndFunction

Function AbortAftermath(String reason)
    If !active
        Return
    EndIf
    ; Invalidate resumed latent stacks before requesting cleanup. They must not
    ; steal again, skip time, fade, or stop a later instance of this quest.
    active = False
    generation += 1
    Phase = 0
    LastResult = reason
    CancelTimer(1)
    UnregisterForRemoteEvent(Game.GetPlayer(), "OnPlayerLoadGame")
    If OSFDefeat.IsDefeated(Game.GetPlayer())
        OSFDefeat.RecoverActorWithTruce(Game.GetPlayer(), TruceSeconds)
        OSFDefeat.ScheduleAutomaticRecovery(Game.GetPlayer(), 2.0)
    EndIf
    Game.FadeOutGame(False, True, 0.0, 0.5)
EndFunction

Event OnQuestShutdown()
    AbortAftermath("interrupted")
EndEvent

Event OnTimer(Int timerID)
    ; Real-time safety net independent of the PassTime-adjusted defeat lease.
    If timerID == 1 && active
        AbortAftermath("timeout")
        Stop()
    EndIf
EndEvent

Event Actor.OnPlayerLoadGame(Actor sender)
    ; A save can resume before or after theft. Do not replay an uncertain phase.
    ; Keep any already-promoted hunt/loot and finish recovery exactly once.
    If active
        AbortAftermath("loaded-blackout")
        Stop()
    EndIf
EndEvent
