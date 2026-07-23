Scriptname OSFDefeatBasicRecovery Hidden
{ Shared aftermath exit for OSF Defeat - Basic Consequences.

  A location patch can place an XMarker in a safe, reachable part of the same
  cell and add OSFDefeatBasicRecoveryMarker to it. We deliberately only use
  markers in the player's current cell: a generic defeat mod must never guess
  a distant destination and break a quest or ship interior. If none is
  available, recovery remains local as the safe fallback. }

Bool Function MoveToNearbyRecoveryMarker(Actor akPlayer) Global
    ; Global functions can't reference script properties (no Self), so the
    ; keyword FormID and search radius are inlined constants, not Properties.
    If akPlayer == None
        Return False
    EndIf

    Keyword kRecoveryMarker = Game.GetFormFromFile(0x00000807, "OSFDefeatBasic.esm") as Keyword
    If kRecoveryMarker == None
        Debug.Trace("[OSFDefeatBasic] recovery marker keyword unavailable; using local recovery")
        Return False
    EndIf

    Cell kPlayerCell = akPlayer.GetParentCell()
    ObjectReference[] kCandidates = akPlayer.FindAllReferencesWithKeyword(kRecoveryMarker, 8192.0)
    ObjectReference kBest = None
    Float fBestDistance = 0.0
    Int i = 0
    While i < kCandidates.Length
        ObjectReference kCandidate = kCandidates[i]
        If kCandidate != None && kCandidate.GetParentCell() == kPlayerCell
            Float fDistance = akPlayer.GetDistance(kCandidate)
            If kBest == None || fDistance < fBestDistance
                kBest = kCandidate
                fBestDistance = fDistance
            EndIf
        EndIf
        i += 1
    EndWhile

    If kBest == None
        Debug.Trace("[OSFDefeatBasic] no same-cell recovery marker found; using local recovery")
        Return False
    EndIf

    Game.FadeOutGame(True, True, 0.0, 0.5, True)
    Utility.Wait(0.6)
    akPlayer.MoveTo(kBest)
    Utility.Wait(0.2)
    Return True
EndFunction

Function FinishAftermath(Actor akPlayer) Global
    Bool bMoved = MoveToNearbyRecoveryMarker(akPlayer)
    OSFDefeat.RescueActor(akPlayer, True)
    If bMoved
        Game.FadeOutGame(False, True, 0.0, 0.5)
    EndIf
EndFunction
