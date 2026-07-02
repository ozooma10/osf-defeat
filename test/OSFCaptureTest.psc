Scriptname OSFCaptureTest Hidden
{ Console test harness for the OSF Defeat M4 capture hooks. NOT shipped content -
  a verification tool. Every function is global, so it is driven from the console
  with callglobalfunction (cgf); no ESM, quest, or attachment required.

  Deploy: drop the compiled OSFCaptureTest.pex into a loaded mod's Scripts folder
  (e.g. MO2\mods\OSF Defeat\Scripts\), boot, then in the console:

      cgf "OSFCaptureTest.Help"

  Typical loop against a live enemy:
      1. Get into a fight, then   cgf "OSFCaptureTest.DownTarget"
         (or enable npcAutoDefeat and beat the NPC down normally)
      2. cgf "OSFCaptureTest.CaptureTarget"     ; claim the downed NPC
      3. cgf "OSFCaptureTest.ListCaptives"      ; see the roster (check the log)
      4. cgf "OSFCaptureTest.TransferToPlayer"  ; the MoveTo verification
      5. cgf "OSFCaptureTest.CheckDistance"     ; confirm they moved to you
      6. cgf "OSFCaptureTest.ReleaseAll"        ; recover everyone

  Each function reports to the notification bar; ListCaptives details go to the
  SFSE log (Debug.Trace / the [OSFDefeat] lines). }

Function Help() global
    Debug.Notification("OSFCaptureTest: DownTarget, CaptureTarget, ListCaptives, TransferToPlayer, CheckDistance, ReleaseAll")
    Debug.Trace("[OSFCaptureTest] cgf: DownTarget CaptureTarget ListCaptives TransferToPlayer CheckDistance ReleaseAll")
EndFunction

; Put the player's current combat target into the down-state, so there is a
; captive candidate even without npcAutoDefeat.
Function DownTarget() global
    Actor player = Game.GetPlayer()
    Actor target = player.GetCombatTarget()
    If target == None
        Debug.Notification("OSFCaptureTest: no combat target (be in a fight)")
        Return
    EndIf
    Bool ok = OSFDefeat.DefeatActor(target, player)
    Debug.Notification("OSFCaptureTest: DownTarget " + target + " queued=" + ok)
EndFunction

; Claim the first non-player defeated NPC as the player's captive.
Function CaptureTarget() global
    Actor npc = FirstDefeatedNpc()
    If npc == None
        Debug.Notification("OSFCaptureTest: no defeated NPC (try DownTarget first)")
        Return
    EndIf
    Bool ok = OSFDefeat.CaptureActor(npc, Game.GetPlayer())
    Debug.Notification("OSFCaptureTest: CaptureActor " + npc + " queued=" + ok)
EndFunction

Function ListCaptives() global
    Actor[] caps = OSFDefeat.GetCaptives(None)
    Debug.Notification("OSFCaptureTest: " + caps.Length + " captive(s) - details in log")
    Int i = 0
    While i < caps.Length
        Actor c = caps[i]
        Debug.Trace("[OSFCaptureTest] captive " + c + " captor=" + OSFDefeat.GetCaptor(c) + " isCaptive=" + OSFDefeat.IsCaptive(c))
        i += 1
    EndWhile
EndFunction

; The MoveTo verification: bring the first captive to the player. Follow with
; CheckDistance to confirm the relocation actually happened.
Function TransferToPlayer() global
    Actor[] caps = OSFDefeat.GetCaptives(None)
    If caps.Length == 0
        Debug.Notification("OSFCaptureTest: no captives to transfer")
        Return
    EndIf
    Actor c = caps[0]
    Float before = Game.GetPlayer().GetDistance(c)
    Bool ok = OSFDefeat.TransferCaptive(c, Game.GetPlayer())
    Debug.Notification("OSFCaptureTest: TransferCaptive " + c + " queued=" + ok + " (was " + before + " away)")
EndFunction

Function CheckDistance() global
    Actor[] caps = OSFDefeat.GetCaptives(None)
    If caps.Length == 0
        Debug.Notification("OSFCaptureTest: no captives")
        Return
    EndIf
    Debug.Notification("OSFCaptureTest: captive[0] distance = " + Game.GetPlayer().GetDistance(caps[0]))
EndFunction

Function ReleaseAll() global
    Actor[] caps = OSFDefeat.GetCaptives(None)
    Int i = 0
    While i < caps.Length
        OSFDefeat.ReleaseCaptive(caps[i], True)
        i += 1
    EndWhile
    Debug.Notification("OSFCaptureTest: released " + caps.Length + " captive(s)")
EndFunction

; First defeated actor that is not the player.
Actor Function FirstDefeatedNpc() global
    Actor player = Game.GetPlayer()
    Actor[] downed = OSFDefeat.GetDefeated(True)
    Int i = 0
    While i < downed.Length
        If downed[i] != player
            Return downed[i]
        EndIf
        i += 1
    EndWhile
    Return None
EndFunction
