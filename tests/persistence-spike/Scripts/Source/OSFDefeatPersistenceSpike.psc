Scriptname OSFDefeatPersistenceSpike extends Quest

; Development only. No defeat registration and no changes to existing world actors.
; Keep reference identity as integers: the quest aliases are the only long-lived
; ObjectReference holders. PlaceAtMe deliberately does NOT force persistence.
ReferenceAlias Property SubjectAlias Auto
ReferenceAlias Property PackAlias Auto
ReferenceAlias Property ShipMarkerAlias Auto
ActorBase Property SubjectBase Auto
Container Property PackBase Auto
Message Property OverrideTitle Auto

String siteKind
Int subjectId
Int packId
Int planetId
Int cellId
Int locationId
Float packX
Float packY
Float packZ
Bool observedAway
Int trips
Int shipId
Int shipCellId
Int shipMarkerId

Function Log(String text)
    Debug.OpenUserLog("OSFDefeatPersistenceSpike")
    Debug.TraceUser("OSFDefeatPersistenceSpike", siteKind + "|" + text)
EndFunction

Bool Function Check(String label, Bool ok)
    If ok
        Log("PASS|" + label)
    Else
        Log("FAIL|" + label)
    EndIf
    Return ok
EndFunction

Function Block(String reason)
    Log("BLOCKED|" + reason)
    Debug.Notification("Persistence spike: " + reason)
EndFunction

Int Function IdOf(Form value)
    If value != None
        Return value.GetFormID()
    EndIf
    Return 0
EndFunction

Function Begin(String kind)
    If subjectId != 0 || packId != 0
        Block("Already initialized. Inspect this run or reload the clean seed.")
        Return
    EndIf
    If kind != "authored" && kind != "generated"
        Block("Begin needs authored or generated.")
        Return
    EndIf
    Actor player = Game.GetPlayer()
    If player.IsInSpace() || player.GetCurrentPlanet() == None || player.GetParentCell().IsInterior()
        Block("Start outside at a planetary site.")
        Return
    EndIf
    If SubjectAlias == None || PackAlias == None || ShipMarkerAlias == None || SubjectBase == None || PackBase == None
        Block("Missing quest properties.")
        Return
    EndIf
    siteKind = kind
    planetId = IdOf(player.GetCurrentPlanet())
    cellId = IdOf(player.GetParentCell())
    locationId = IdOf(player.GetCurrentLocation())
    Actor subject = player.PlaceAtMe(SubjectBase, 1, False, True, False, abSnapOffsetToNavmesh = False) as Actor
    ObjectReference pack = player.PlaceAtMe(PackBase, 1, False, True, False, abSnapOffsetToNavmesh = False)
    ; Save IDs before checking, so Cleanup can remove a partially created fixture.
    subjectId = IdOf(subject)
    packId = IdOf(pack)
    If subject != None
        SubjectAlias.ForceRefTo(subject)
    EndIf
    If pack != None
        PackAlias.ForceRefTo(pack)
    EndIf
    If subject == None || pack == None
        Block("Fixture creation failed. Run Cleanup, then reload the seed.")
        Return
    EndIf
    subject.EnableAI(False)
    subject.SetGhost(True)
    subject.MoveTo(player, 2.0, 0.0, 0.0)
    pack.MoveTo(player, -2.0, 0.0, 0.0)
    MiscObject credits = Game.GetCredits()
    subject.RemoveItem(credits, subject.GetItemCount(credits), True)
    subject.AddItem(credits, 23, True)
    pack.RemoveAllItems()
    pack.AddItem(credits, 137, True)
    subject.Enable()
    pack.Enable()
    packX = pack.GetPositionX()
    packY = pack.GetPositionY()
    packZ = pack.GetPositionZ()
    SetObjectiveDisplayed(10)
    SetObjectiveDisplayed(20)
    Log("BEGIN|actor=" + subjectId + "|pack=" + packId + "|planet=" + planetId + "|cell=" + cellId + "|location=" + locationId)
    Inspect("baseline")
    Debug.Notification("Spike ready. Check actor name and journal, then save.")
EndFunction

Bool Function Inspect(String checkpoint = "inspection")
    If subjectId == 0 || packId == 0
        Block("No complete fixture. Run Begin at the test site.")
        Return False
    EndIf
    Log("INSPECT|" + checkpoint + "|trips=" + trips)
    Actor subject = SubjectAlias.GetActorReference()
    ObjectReference pack = PackAlias.GetReference()
    Bool ok = Check("actor-identity", subject != None && IdOf(subject) == subjectId)
    ok = Check("pack-identity", pack != None && IdOf(pack) == packId) && ok
    If subject == None || pack == None
        Return False
    EndIf
    ok = Check("actor-alive-enabled-loaded", !subject.IsDead() && !subject.IsDisabled() && !subject.IsDeleted() && subject.Is3DLoaded()) && ok
    ok = Check("pack-enabled-loaded", !pack.IsDisabled() && !pack.IsDeleted() && pack.Is3DLoaded()) && ok
    ok = Check("original-cell", IdOf(subject.GetParentCell()) == cellId && IdOf(pack.GetParentCell()) == cellId) && ok
    ok = Check("original-planet", IdOf(subject.GetCurrentPlanet()) == planetId && IdOf(pack.GetCurrentPlanet()) == planetId) && ok
    ok = Check("actor-inventory", subject.GetItemCount(Game.GetCredits()) == 23) && ok
    ok = Check("pack-inventory", pack.GetItemCount(Game.GetCredits()) == 137) && ok
    ok = Check("pack-position", Math.Abs(pack.GetPositionX() - packX) < 0.5 && Math.Abs(pack.GetPositionY() - packY) < 0.5 && Math.Abs(pack.GetPositionZ() - packZ) < 0.5) && ok
    ok = Check("objectives-visible", IsObjectiveDisplayed(10) && IsObjectiveDisplayed(20)) && ok
    Log("VISUAL-REQUIRED|actor name, journal alias substitution, both objective markers")
    If ok
        Debug.Notification("Spike inspection passed; record the visual checks separately.")
    Else
        Debug.Notification("Spike inspection FAILED. Keep the log and save.")
    EndIf
    Return ok
EndFunction

Function MarkAway()
    If subjectId == 0 || packId == 0 || observedAway
        Block("Need an initialized fixture and no pending return.")
        Return
    EndIf
    Actor player = Game.GetPlayer()
    Cell origin = Game.GetForm(cellId) as Cell
    If player.GetCurrentPlanet() == None || IdOf(player.GetCurrentPlanet()) == planetId || player.IsInSpace()
        Block("Land on a different planet before MarkAway.")
        Return
    EndIf
    If origin == None || origin.IsAttached()
        Block("Original cell has not been observed detached.")
        Return
    EndIf
    ObjectReference subject = SubjectAlias.GetReference()
    ObjectReference pack = PackAlias.GetReference()
    If subject == None || pack == None
        Check("aliases-present-away", False)
        Return
    EndIf
    If subject.Is3DLoaded() || pack.Is3DLoaded()
        Block("Test references still have loaded 3D.")
        Return
    EndIf
    observedAway = True
    Log("AWAY|planet=" + IdOf(player.GetCurrentPlanet()) + "|origin-detached|actor-and-pack-unloaded")
    Debug.Notification("Departure recorded. Return through the original landing site.")
EndFunction

Function CheckReturn()
    If !observedAway
        Block("No observed departure. Run MarkAway on another planet first.")
        Return
    EndIf
    Actor player = Game.GetPlayer()
    ObjectReference pack = PackAlias.GetReference()
    If IdOf(player.GetCurrentPlanet()) != planetId || IdOf(player.GetParentCell()) != cellId || pack == None
        Block("Return to the original test site before CheckReturn.")
        Return
    EndIf
    If player.GetDistance(pack) > 20.0
        Block("Stand near the original pack before CheckReturn.")
        Return
    EndIf
    If Inspect("planet-return")
        trips += 1
        observedAway = False
        Log("RETURN|trip=" + trips)
    EndIf
EndFunction

Function TryOverrideName()
    Actor subject = SubjectAlias.GetActorReference()
    If subject == None || OverrideTitle == None
        Block("Need a subject and override Message.")
        Return
    EndIf
    subject.SetOverrideName(OverrideTitle)
    Log("VISUAL-REQUIRED|SetOverrideName requested Persistence Override; inspect actor and journal")
EndFunction

Function RememberShip()
    Actor player = Game.GetPlayer()
    SpaceshipReference ship = Game.GetPlayerHomeSpaceShip()
    If ship == None || player.GetCurrentShipRef() != ship || !ship.IsLanded() || !player.GetParentCell().IsInterior()
        Block("Stand inside your landed home ship at a safe floor position.")
        Return
    EndIf
    If shipMarkerId != 0
        Block("Ship marker already captured. Reload the seed for another ship.")
        Return
    EndIf
    ObjectReference marker = player.PlaceAtMe(Game.GetFormFromFile(0x3B, "Starfield.esm"), 1, False, False, False, abSnapOffsetToNavmesh = False)
    If marker == None
        Block("Could not create ship marker.")
        Return
    EndIf
    ShipMarkerAlias.ForceRefTo(marker)
    shipId = IdOf(ship)
    shipCellId = IdOf(player.GetParentCell())
    shipMarkerId = IdOf(marker)
    Log("SHIP-MARKER|ship=" + shipId + "|cell=" + shipCellId + "|marker=" + shipMarkerId)
    Debug.Notification("Ship marker captured. Exit the ship, then run TestShip.")
EndFunction

Function TestShip()
    Actor player = Game.GetPlayer()
    SpaceshipReference ship = Game.GetPlayerHomeSpaceShip()
    ObjectReference marker = ShipMarkerAlias.GetReference()
    If marker == None || IdOf(marker) != shipMarkerId || ship == None || IdOf(ship) != shipId
        Block("Missing or changed ship/marker. Run RememberShip first.")
        Return
    EndIf
    If !ship.IsLanded() || player.IsInSpace() || ship.GetCurrentPlanet() == None || player.GetCurrentPlanet() != ship.GetCurrentPlanet()
        Block("Home ship must be landed on your current planet.")
        Return
    EndIf
    If player.GetParentCell().IsInterior() || IdOf(marker.GetParentCell()) != shipCellId
        Block("Start outside; the destination must remain in its captured interior.")
        Return
    EndIf
    Log("SHIP-MOVE-BEGIN|from-cell=" + IdOf(player.GetParentCell()) + "|to-cell=" + shipCellId)
    player.MoveTo(marker)
    Utility.Wait(2.0)
    Bool ok = Check("ship-move-interior", IdOf(player.GetParentCell()) == shipCellId && player.GetParentCell().IsInterior())
    ok = Check("ship-move-correct-ship", player.GetCurrentShipRef() == ship) && ok
    ok = Check("ship-move-position", player.GetDistance(marker) < 2.0) && ok
    If ok
        Debug.Notification("Ship MoveTo passed. Check movement, doors, and camera.")
    Else
        Debug.Notification("Ship MoveTo FAILED. Keep evidence, then reload the test save.")
    EndIf
    Log("VISUAL-REQUIRED|ship arrival, controls, camera, exit door")
EndFunction

Function DeleteOwned(ReferenceAlias slot, Int expectedId)
    ObjectReference value = slot.GetReference()
    If value != None && expectedId != 0 && IdOf(value) == expectedId
        slot.Clear()
        value.Disable()
        value.Delete()
    EndIf
EndFunction

Function Cleanup()
    DeleteOwned(SubjectAlias, subjectId)
    DeleteOwned(PackAlias, packId)
    DeleteOwned(ShipMarkerAlias, shipMarkerId)
    SetObjectiveDisplayed(10, False)
    SetObjectiveDisplayed(20, False)
    Log("CLEANUP|reload the clean seed; do not use this save for normal play")
    Stop()
EndFunction
