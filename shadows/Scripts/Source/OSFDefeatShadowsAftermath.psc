Scriptname OSFDefeatShadowsAftermath extends Quest

OSFDefeatShadowsHunt Property Hunt Auto Const Mandatory
Int Property BlackoutHours = 2 Auto Const
Float Property TruceSeconds = 30.0 Auto Const

Event OnQuestInit()
    RunAftermath()
EndEvent

Function RunAftermath()
    Actor player = Game.GetPlayer()
    Actor aggressor = OSFDefeat.GetLastPlayerAggressor()
    If !OSFDefeat.IsDefeated(player)
        Stop()
        Return
    EndIf
    ; The on-foot domain describes the victim, not their cell. Refuse blackout
    ; in ship interiors or space, where a future return is not yet established.
    If player.IsInSpaceEx(True) || player.GetCurrentShipRef() != None
        OSFDefeat.RecoverActor(player)
        Int retries = 0
        While OSFDefeat.IsDefeated(player) && retries < 50
            Utility.Wait(0.1)
            retries += 1
        EndWhile
        If !OSFDefeat.IsDefeated(player)
            OSFDefeat.ResistDefeat(player, 30.0)
            Utility.Wait(0.1)
            player.Kill(aggressor)
        EndIf
        Stop()
        Return
    EndIf
    If !Hunt.IsRunning()
        Hunt.Start()
    EndIf
    ; PassTime advances the framework's game-time watchdog too. The registration
    ; leases 12,000 seconds (over three game hours even at time scale 1) and a real
    ; Papyrus timer below recovers/fades in if any latent step gets stuck.
    RegisterForRemoteEvent(player, "OnPlayerLoadGame")
    StartTimer(30.0, 1)
    Game.FadeOutGame(True, True, 0.0, 0.6, True)
    Utility.Wait(0.7)
    Bool promoted = Hunt.Promote(aggressor)
    Float wakeDay = Utility.GetCurrentGameTime() + (BlackoutHours / 24.0)
    Game.PassTime(BlackoutHours)
    ; PassTime lands on an hour boundary and can advance less than requested.
    ; One additional hour ensures at least the requested elapsed time.
    If Utility.GetCurrentGameTime() < wakeDay
        Game.PassTime(1)
    EndIf
    OSFDefeat.RecoverActorWithTruce(player, TruceSeconds)
    Int tries = 0
    While OSFDefeat.IsDefeated(player) && tries < 50
        Utility.Wait(0.1)
        tries += 1
    EndWhile
    If OSFDefeat.IsDefeated(player)
        ; Never strand the player behind a fade if the native request was refused.
        OSFDefeat.RecoverActor(player)
    EndIf
    Game.FadeOutGame(False, True, 0.0, 0.8)
    CancelTimer(1)
    UnregisterForRemoteEvent(player, "OnPlayerLoadGame")
    If promoted
        Debug.Notification("Hours later. Your attacker left a wound and a trail. Check your journal.")
    Else
        Debug.Notification("Hours later. You have a brief chance to get away.")
    EndIf
    Stop()
EndFunction

Event OnTimer(Int timerID)
    ; Real-time safety net independent of the PassTime-adjusted defeat lease.
    OSFDefeat.RecoverActorWithTruce(Game.GetPlayer(), TruceSeconds)
    Game.FadeOutGame(False, True, 0.0, 0.5)
EndEvent

Event Actor.OnPlayerLoadGame(Actor sender)
    ; Loaded Papyrus stacks resume, but the visual fade may not. Always uncover.
    Game.FadeOutGame(False, True, 0.0, 0.5)
    StartTimer(10.0, 1)
EndEvent
