Scriptname OSFDefeatPersistenceSpike extends Quest

; Development only. No defeat registration and no changes to existing world actors.
; The aliases are the only long-lived ObjectReference holders. Identity is a
; per-instance AV stamp written only when creating the fixture, never on load.
; Raw IDs are diagnostics: dynamic IDs remap across process restarts.
; Cell/Planet forms use the save system's form remapping, not Game.GetForm(int).
; PlaceAtMe deliberately does NOT force persistence.
ReferenceAlias Property SubjectAlias Auto
ReferenceAlias Property PackAlias Auto
ReferenceAlias Property ShipMarkerAlias Auto
ActorBase Property SubjectBase Auto
Container Property PackBase Auto
Message Property OverrideTitle Auto
ActorValue Property IdentityStamp Auto

String siteKind
Bool initialized
Int subjectStamp
Int packStamp
Int markerStamp
Int subjectId
Int packId
Planet originPlanet
Cell originCell
Int initialCellId
Float packX
Float packY
Float packZ
Bool observedAway
Bool subjectDetached
Bool packDetached
Int trips
Int shipId
Cell shipCell
Int shipMarkerId
Float awayDay

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

; A saved Cell script handle can remain non-None while its native cell is
; unbound off-planet. Observe detachment on the live aliases instead of calling
; native Cell functions on that handle. Keep the Cell for comparison on return.
Event ReferenceAlias.OnCellAttach(ReferenceAlias sender)
    If sender == SubjectAlias
        subjectDetached = False
        Log("CELL-ATTACH|actor")
    ElseIf sender == PackAlias
        packDetached = False
        Log("CELL-ATTACH|pack")
    EndIf
EndEvent

Event ReferenceAlias.OnCellDetach(ReferenceAlias sender)
    If sender == SubjectAlias
        subjectDetached = True
        Log("CELL-DETACH|actor")
    ElseIf sender == PackAlias
        packDetached = True
        Log("CELL-DETACH|pack")
    EndIf
EndEvent

Int Function IdOf(Form value)
    If value != None
        Return value.GetFormID()
    EndIf
    Return 0
EndFunction

Bool Function Owns(ObjectReference value, Int stamp, Form expectedBase)
    If value == None || stamp == 0 || IdentityStamp == None || expectedBase == None
        Return False
    EndIf
    Return value.GetBaseObject() == expectedBase && value.GetValue(IdentityStamp) == stamp
EndFunction

; A new object of the same base must not pass the identity oracle. The temporary
; control never enters an alias, survives a save, or receives the expected stamp.
Bool Function CheckIdentityControl(Form baseForm, Int stamp, String label)
    ObjectReference control = Game.GetPlayer().PlaceAtMe(baseForm, 1, False, True, False, abSnapOffsetToNavmesh = False)
    If control == None
        Return Check(label + "-control-created", False)
    EndIf
    Bool ok = Check(label + "-rejects-unstamped", !Owns(control, stamp, baseForm))
    control.SetValue(IdentityStamp, stamp + 1)
    ok = Check(label + "-rejects-wrong-stamp", !Owns(control, stamp, baseForm)) && ok
    control.Delete()
    Return ok
EndFunction

Function Begin(String kind)
    If initialized
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
    If SubjectAlias == None || PackAlias == None || ShipMarkerAlias == None || SubjectBase == None || PackBase == None || IdentityStamp == None
        Block("Missing quest properties.")
        Return
    EndIf
    initialized = True
    siteKind = kind
    RegisterForRemoteEvent(SubjectAlias, "OnCellAttach")
    RegisterForRemoteEvent(SubjectAlias, "OnCellDetach")
    RegisterForRemoteEvent(PackAlias, "OnCellAttach")
    RegisterForRemoteEvent(PackAlias, "OnCellDetach")
    originPlanet = player.GetCurrentPlanet()
    originCell = player.GetParentCell()
    initialCellId = IdOf(originCell)
    ; All values remain exact integers in a 32-bit float, with disjoint role ranges.
    subjectStamp = Utility.RandomInt(1000000, 3999999)
    packStamp = subjectStamp + 4000000
    markerStamp = subjectStamp + 8000000
    Actor subject = player.PlaceAtMe(SubjectBase, 1, False, True, False, abSnapOffsetToNavmesh = False) as Actor
    ObjectReference pack = player.PlaceAtMe(PackBase, 1, False, True, False, abSnapOffsetToNavmesh = False)
    ; Stamp before alias fill, so even a partially created fixture can be cleaned.
    subjectId = IdOf(subject)
    packId = IdOf(pack)
    If subject != None
        subject.SetValue(IdentityStamp, subjectStamp)
        SubjectAlias.ForceRefTo(subject)
    EndIf
    If pack != None
        pack.SetValue(IdentityStamp, packStamp)
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
    Log("BEGIN|version=3|actor=" + subjectId + "|pack=" + packId + "|planet=" + IdOf(originPlanet) + "|cell=" + initialCellId + "|subject-stamp=" + subjectStamp + "|pack-stamp=" + packStamp)
    CheckIdentityControl(SubjectBase, subjectStamp, "actor")
    CheckIdentityControl(PackBase, packStamp, "pack")
    Inspect("baseline")
    Debug.Notification("Spike ready. Check actor name and journal, then save.")
EndFunction

Bool Function Inspect(String checkpoint = "inspection")
    If !initialized || subjectId == 0 || packId == 0
        Block("No complete fixture. Run Begin at the test site.")
        Return False
    EndIf
    Log("INSPECT|" + checkpoint + "|trips=" + trips)
    Actor subject = SubjectAlias.GetActorReference()
    ObjectReference pack = PackAlias.GetReference()
    Log("IDS|actor-initial=" + subjectId + "|actor-now=" + IdOf(subject) + "|pack-initial=" + packId + "|pack-now=" + IdOf(pack) + "|cell-initial=" + initialCellId + "|cell-now=" + IdOf(originCell))
    Bool ok = Check("actor-identity", Owns(subject, subjectStamp, SubjectBase))
    ok = Check("pack-identity", Owns(pack, packStamp, PackBase)) && ok
    If subject == None || pack == None
        Return False
    EndIf
    ok = Check("actor-alive-enabled-loaded", !subject.IsDead() && !subject.IsDisabled() && !subject.IsDeleted() && subject.Is3DLoaded()) && ok
    ok = Check("pack-enabled-loaded", !pack.IsDisabled() && !pack.IsDeleted() && pack.Is3DLoaded()) && ok
    ok = Check("original-cell", originCell != None && subject.GetParentCell() == originCell && pack.GetParentCell() == originCell) && ok
    ok = Check("original-planet", originPlanet != None && subject.GetCurrentPlanet() == originPlanet && pack.GetCurrentPlanet() == originPlanet) && ok
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
    If player.GetCurrentPlanet() == None || originPlanet == None || player.GetCurrentPlanet() == originPlanet || player.IsInSpace()
        Block("Land on a different planet before MarkAway.")
        Return
    EndIf
    If originCell == None
        Block("Saved origin Cell form is missing.")
        Return
    EndIf
    ObjectReference subject = SubjectAlias.GetReference()
    ObjectReference pack = PackAlias.GetReference()
    If subject == None || pack == None
        Check("aliases-present-away", False)
        Return
    EndIf
    Log("AWAY-OBSERVATION|actor-detached=" + subjectDetached + "|pack-detached=" + packDetached + "|actor-3d=" + subject.Is3DLoaded() + "|pack-3d=" + pack.Is3DLoaded())
    Bool ok = Check("actor-identity-away", Owns(subject, subjectStamp, SubjectBase))
    ok = Check("pack-identity-away", Owns(pack, packStamp, PackBase)) && ok
    ok = Check("both-cell-detach-events", subjectDetached && packDetached) && ok
    If !ok
        Return
    EndIf
    If subject.Is3DLoaded() || pack.Is3DLoaded()
        Block("Test references still have loaded 3D.")
        Return
    EndIf
    observedAway = True
    awayDay = Utility.GetCurrentGameTime()
    Log("AWAY|planet=" + IdOf(player.GetCurrentPlanet()) + "|alias-cell-detach-observed|actor-and-pack-unloaded")
    Debug.Notification("Departure recorded. Return through the original landing site.")
EndFunction

Function CheckReturn()
    If !observedAway
        Block("No observed departure. Run MarkAway on another planet first.")
        Return
    EndIf
    Actor player = Game.GetPlayer()
    ObjectReference pack = PackAlias.GetReference()
    If player.GetCurrentPlanet() != originPlanet || originCell == None || player.GetParentCell() != originCell || pack == None
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
        Log("RETURN|trip=" + trips + "|elapsed-UT-hours=" + ((Utility.GetCurrentGameTime() - awayDay) * 24.0))
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
    If !initialized
        Block("Run Begin before capturing a ship marker.")
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
    marker.SetValue(IdentityStamp, markerStamp)
    ShipMarkerAlias.ForceRefTo(marker)
    shipId = IdOf(ship)
    shipCell = player.GetParentCell()
    shipMarkerId = IdOf(marker)
    Log("SHIP-MARKER|ship=" + shipId + "|cell=" + IdOf(shipCell) + "|marker=" + shipMarkerId + "|stamp=" + markerStamp)
    Debug.Notification("Ship marker captured. Exit the ship, then run TestShip.")
EndFunction

Function TestShip()
    Actor player = Game.GetPlayer()
    SpaceshipReference ship = Game.GetPlayerHomeSpaceShip()
    ObjectReference marker = ShipMarkerAlias.GetReference()
    If !Owns(marker, markerStamp, Game.GetFormFromFile(0x3B, "Starfield.esm")) || ship == None
        Block("Missing or changed ship/marker. Run RememberShip first.")
        Return
    EndIf
    If marker.GetCurrentShipRef() != ship
        Block("Captured marker is not inside the current home ship.")
        Return
    EndIf
    If !ship.IsLanded() || player.IsInSpace() || ship.GetCurrentPlanet() == None || player.GetCurrentPlanet() != ship.GetCurrentPlanet()
        Block("Home ship must be landed on your current planet.")
        Return
    EndIf
    If player.GetParentCell().IsInterior() || shipCell == None || marker.GetParentCell() != shipCell
        Block("Start outside; the destination must remain in its captured interior.")
        Return
    EndIf
    Log("SHIP-MOVE-BEGIN|from-cell=" + IdOf(player.GetParentCell()) + "|to-cell=" + IdOf(shipCell) + "|marker-initial=" + shipMarkerId + "|marker-now=" + IdOf(marker) + "|ship-initial=" + shipId + "|ship-now=" + IdOf(ship))
    player.MoveTo(marker)
    Utility.Wait(2.0)
    Bool ok = Check("ship-move-interior", player.GetParentCell() == shipCell && player.GetParentCell().IsInterior())
    ok = Check("ship-move-correct-ship", player.GetCurrentShipRef() == ship) && ok
    ok = Check("ship-move-position", player.GetDistance(marker) < 2.0) && ok
    If ok
        Debug.Notification("Ship MoveTo passed. Check movement, doors, and camera.")
    Else
        Debug.Notification("Ship MoveTo FAILED. Keep evidence, then reload the test save.")
    EndIf
    Log("VISUAL-REQUIRED|ship arrival, controls, camera, exit door")
EndFunction

Function DeleteOwned(ReferenceAlias slot, Int stamp, Form expectedBase)
    ObjectReference value = slot.GetReference()
    If Owns(value, stamp, expectedBase)
        slot.Clear()
        value.Disable()
        value.Delete()
        Log("CLEANUP-DELETED|stamp=" + stamp)
    ElseIf value != None
        Block("Cleanup refused a reference without its original identity stamp.")
    EndIf
EndFunction

Function Cleanup()
    DeleteOwned(SubjectAlias, subjectStamp, SubjectBase)
    DeleteOwned(PackAlias, packStamp, PackBase)
    DeleteOwned(ShipMarkerAlias, markerStamp, Game.GetFormFromFile(0x3B, "Starfield.esm"))
    SetObjectiveDisplayed(10, False)
    SetObjectiveDisplayed(20, False)
    Log("CLEANUP|reload the clean seed; do not use this save for normal play")
    Stop()
EndFunction
