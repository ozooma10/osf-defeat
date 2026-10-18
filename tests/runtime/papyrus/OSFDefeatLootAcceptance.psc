Scriptname OSFDefeatLootAcceptance extends ObjectReference

OSFDefeatShadowsHunt hunt
Actor target
Weapon eon
Weapon grendel
Weapon lostWeapon
Ammo ammunition
Armor suit
Armor helmet
Armor pack
Int funds
Int expectedTaken
Int expectedStatus
Int playerWeapons
Int targetWeapons
Int targetAmmo
Int targetCredits
Int targetSuits
Int targetHelmets
Int targetPacks

Function Initialize()
    hunt = Game.GetFormFromFile(0x801, "OSFDefeatShadows.esm") as OSFDefeatShadowsHunt
    eon = Game.GetFormFromFile(0x476C4, "Starfield.esm") as Weapon
    grendel = Game.GetFormFromFile(0x28A02, "Starfield.esm") as Weapon
    ; Installed .244 ARMO editor IDs: Spacesuit_Miner, _Helmet, _Backpack.
    suit = Game.GetFormFromFile(0x5278E, "Starfield.esm") as Armor
    helmet = Game.GetFormFromFile(0x52792, "Starfield.esm") as Armor
    pack = Game.GetFormFromFile(0x2EDF1F, "Starfield.esm") as Armor
    If eon != None
        ammunition = eon.GetAmmo()
    EndIf
    OSFDefeatAcceptanceProbe.Record("loot-fixture", hunt != None && eon != None && grendel != None && ammunition != None && suit != None && helmet != None && pack != None)
EndFunction

Actor Function NewTarget()
    Int[] bases = new Int[10]
    bases[0] = 0x270255
    bases[1] = 0x270254
    bases[2] = 0x270253
    bases[3] = 0x270252
    bases[4] = 0x129583
    bases[5] = 0x129582
    bases[6] = 0x129581
    bases[7] = 0x129580
    bases[8] = 0x12957F
    bases[9] = 0x12957E
    Int i = 0
    While i < bases.Length
        Actor candidate = Game.GetPlayer().PlaceAtMe(Game.GetFormFromFile(bases[i], "Starfield.esm"), 1, False, True, False) as Actor
        If candidate != None
            If hunt.Eligible(candidate)
                candidate.Enable()
                candidate.MoveTo(Game.GetPlayer(), 8.0, 0.0, 0.0)
                candidate.SetRestrained(True)
                OSFDefeat.SuspendActorAI(candidate)
                Utility.Wait(0.5)
                Return candidate
            EndIf
            candidate.Delete()
        EndIf
        i += 1
    EndWhile
    Return None
EndFunction

Function SetCount(Actor owner, Form item, Int count)
    owner.RemoveItem(item, owner.GetItemCount(item), True)
    If count > 0
        owner.AddItem(item, count, True)
    EndIf
EndFunction

Function Prepare(String kind)
    ; Repair only after the preceding case has recorded its observations.
    hunt.CancelHunt()
    If target != None
        target.RemoveSpell(hunt.NemesisBuff)
        OSFDefeat.ResumeActorAI(target)
        target.Disable()
        target.Delete()
    EndIf
    If !hunt.IsRunning()
        hunt.Start()
    EndIf
    Utility.Wait(0.5)
    target = NewTarget()
    If target == None
        OSFDefeatAcceptanceProbe.Record("loot-ready-" + kind, False)
        Return
    EndIf
    Actor player = Game.GetPlayer()
    player.UnequipAll()
    Weapon old = target.GetEquippedWeapon()
    If old != None
        target.UnequipItem(old, True, True)
        SetCount(target, old, 0)
    EndIf
    SetCount(target, eon, 0)
    SetCount(target, grendel, 0)
    SetCount(player, grendel, 0)
    SetCount(player, eon, 1)
    SetCount(target, ammunition, 0)
    funds = 1000
    expectedTaken = 100
    expectedStatus = 1
    lostWeapon = None
    If kind == "credits-0"
        funds = 0
        expectedTaken = 0
    ElseIf kind == "credits-1"
        funds = 1
        expectedTaken = 1
    ElseIf kind == "credits-24"
        funds = 24
        expectedTaken = 24
    ElseIf kind == "credits-25"
        funds = 25
        expectedTaken = 25
    ElseIf kind == "credits-26"
        funds = 26
        expectedTaken = 25
    ElseIf kind == "credits-249"
        funds = 249
        expectedTaken = 25
    ElseIf kind == "credits-259"
        funds = 259
        expectedTaken = 25
    ElseIf kind == "credits-24999"
        funds = 24999
        expectedTaken = 2499
    ElseIf kind == "credits-25000"
        funds = 25000
        expectedTaken = 2500
    ElseIf kind == "credits-25010"
        funds = 25010
        expectedTaken = 2500
    ElseIf kind != "unarmed"
        lostWeapon = eon
        expectedStatus = 7
        If kind == "missing-weapon"
            SetCount(player, eon, 0)
            expectedStatus = 2
        ElseIf kind == "duplicate-weapon"
            SetCount(player, eon, 2)
            expectedStatus = 2
        ElseIf kind == "equal-weapon"
            target.AddItem(eon, 1, True)
            target.AddItem(ammunition, 12, True)
            target.EquipItem(eon, False, True)
            expectedStatus = 3
        ElseIf kind == "better-weapon"
            target.AddItem(grendel, 1, True)
            target.AddItem(grendel.GetAmmo(), 12, True)
            target.EquipItem(grendel, False, True)
            expectedStatus = 3
        ElseIf kind == "ammo-present"
            SetCount(target, ammunition, 12)
        EndIf
        If player.GetItemCount(eon) > 0
            player.EquipItem(eon, False, True)
        EndIf
    EndIf
    SetCount(player, Game.GetCredits(), funds)
    If kind == "armor"
        SetCount(player, suit, 1)
        SetCount(player, helmet, 1)
        SetCount(player, pack, 1)
        player.EquipItem(suit, False, True)
        player.EquipItem(helmet, False, True)
        player.EquipItem(pack, False, True)
    EndIf
    ; AddItem on a weapon can also grant its loaded ammunition (the duplicate
    ; fixture added 32 rounds). Normalize after ALL weapon additions/equipment.
    SetCount(player, ammunition, 37)
    Utility.Wait(1.0)
    playerWeapons = player.GetItemCount(eon)
    targetWeapons = target.GetItemCount(eon)
    targetAmmo = target.GetItemCount(ammunition)
    targetCredits = target.GetItemCount(Game.GetCredits())
    targetSuits = target.GetItemCount(suit)
    targetHelmets = target.GetItemCount(helmet)
    targetPacks = target.GetItemCount(pack)
    Bool ready = hunt.IsRunning() && hunt.Nemesis.GetReference() == None && hunt.StolenWeapon.GetReference() == None && hunt.Eligible(target) && player.GetItemCount(Game.GetCredits()) == funds && player.GetItemCount(ammunition) == 37
    If kind == "missing-weapon"
        ready = ready && playerWeapons == 0
    ElseIf kind == "duplicate-weapon"
        ready = ready && playerWeapons == 2 && player.GetEquippedWeapon() == eon
    Else
        ready = ready && playerWeapons == 1
        If lostWeapon != None
            ready = ready && player.GetEquippedWeapon() == eon
        Else
            ready = ready && !player.IsEquipped(eon)
        EndIf
    EndIf
    If kind == "equal-weapon"
        ready = ready && target.GetEquippedWeapon() == eon && targetWeapons == 1
    ElseIf kind == "better-weapon"
        ready = ready && target.GetEquippedWeapon() == grendel && target.GetItemCount(grendel) == 1 && grendel.GetGoldValue() > eon.GetGoldValue()
    Else
        Weapon theirs = target.GetEquippedWeapon()
        ready = ready && (theirs == None || target.GetItemCount(theirs) == 0)
    EndIf
    If kind == "armor"
        ready = ready && ArmorPreserved()
    EndIf
    hunt.LastTheftStatus = -1
    hunt.LastWeaponConsidered = None
    String setup = "loot-setup-" + kind
    OSFDefeatAcceptanceProbe.Record(setup + "-running", hunt.IsRunning())
    OSFDefeatAcceptanceProbe.Record(setup + "-nemesis-empty", hunt.Nemesis.GetReference() == None)
    OSFDefeatAcceptanceProbe.Record(setup + "-weapon-empty", hunt.StolenWeapon.GetReference() == None)
    OSFDefeatAcceptanceProbe.Record(setup + "-eligible", hunt.Eligible(target), target)
    OSFDefeatAcceptanceProbe.Record(setup + "-credits", player.GetItemCount(Game.GetCredits()) == funds, player, player.GetItemCount(Game.GetCredits()))
    OSFDefeatAcceptanceProbe.Record(setup + "-player-ammo", player.GetItemCount(ammunition) == 37, player, player.GetItemCount(ammunition))
    OSFDefeatAcceptanceProbe.Record(setup + "-player-weapons", True, player, playerWeapons)
    Weapon equipped = target.GetEquippedWeapon()
    If equipped != None
        OSFDefeatAcceptanceProbe.Record(setup + "-target-weapon-form", True, target, equipped.GetFormID())
        OSFDefeatAcceptanceProbe.Record(setup + "-target-weapon-count", True, target, target.GetItemCount(equipped))
    EndIf
    If kind == "armor"
        OSFDefeatAcceptanceProbe.Record(setup + "-armor", ArmorPreserved())
    EndIf
    OSFDefeatAcceptanceProbe.Record("loot-ready-" + kind, ready, target)
EndFunction

Bool Function ArmorPreserved()
    Actor player = Game.GetPlayer()
    Return player.GetItemCount(suit) == 1 && player.GetItemCount(helmet) == 1 && player.GetItemCount(pack) == 1 && player.IsEquipped(suit) && player.IsEquipped(helmet) && player.IsEquipped(pack) && target.GetItemCount(suit) == targetSuits && target.GetItemCount(helmet) == targetHelmets && target.GetItemCount(pack) == targetPacks
EndFunction

Function Run(String kind)
    Actor player = Game.GetPlayer()
    Bool promoted = hunt.Promote(target, lostWeapon)
    Utility.Wait(0.5)
    String label = "loot-observed-" + kind
    Bool active = promoted && hunt.Nemesis.GetActorReference() == target && target.HasSpell(hunt.NemesisBuff) && player.HasSpell(hunt.PlayerDebuff) && hunt.IsObjectiveDisplayed(10)
    Bool creditsOk = player.GetItemCount(Game.GetCredits()) == funds - expectedTaken && target.GetItemCount(Game.GetCredits()) == targetCredits + expectedTaken
    Bool weaponOk = hunt.LastTheftStatus == expectedStatus && hunt.LastWeaponConsidered == lostWeapon
    Bool ammoOk = player.GetItemCount(ammunition) == 37
    If expectedStatus == 7
        ObjectReference stolen = hunt.StolenWeapon.GetReference()
        weaponOk = weaponOk && stolen != None && stolen.GetBaseObject() == eon && player.GetItemCount(eon) == playerWeapons - 1 && target.GetItemCount(eon) == targetWeapons + 1 && target.GetEquippedWeapon() == eon
        Int expectedAmmo = targetAmmo
        If expectedAmmo == 0
            expectedAmmo = 1
        EndIf
        ammoOk = ammoOk && target.GetItemCount(ammunition) == expectedAmmo
    Else
        weaponOk = weaponOk && hunt.StolenWeapon.GetReference() == None && player.GetItemCount(eon) == playerWeapons && target.GetItemCount(eon) == targetWeapons
        ammoOk = ammoOk && target.GetItemCount(ammunition) == targetAmmo
        If kind == "equal-weapon"
            weaponOk = weaponOk && target.GetEquippedWeapon() == eon
        ElseIf kind == "better-weapon"
            weaponOk = weaponOk && target.GetItemCount(grendel) == 1 && target.GetEquippedWeapon() == grendel
        EndIf
    EndIf
    Bool armorOk = kind != "armor" || ArmorPreserved()
    OSFDefeatAcceptanceProbe.Record(label + "-promoted", active, target)
    OSFDefeatAcceptanceProbe.Record(label + "-player-credits", creditsOk, player, player.GetItemCount(Game.GetCredits()))
    OSFDefeatAcceptanceProbe.Record(label + "-target-credits", creditsOk, target, target.GetItemCount(Game.GetCredits()))
    OSFDefeatAcceptanceProbe.Record(label + "-weapon", weaponOk, target, hunt.LastTheftStatus)
    OSFDefeatAcceptanceProbe.Record(label + "-ammo", ammoOk, target, target.GetItemCount(ammunition))
    OSFDefeatAcceptanceProbe.Record(label + "-armor", armorOk)
    OSFDefeatAcceptanceProbe.Record("loot-" + kind, active && creditsOk && weaponOk && ammoOk && armorOk, target)
EndFunction

Function InspectCleanup()
    Utility.Wait(2.0)
    Actor player = Game.GetPlayer()
    hunt = Game.GetFormFromFile(0x801, "OSFDefeatShadows.esm") as OSFDefeatShadowsHunt
    OSFDefeatAcceptanceProbe.Record("loot-cleanup", !player.HasSpell(hunt.PlayerDebuff) && !player.HasSpell(hunt.PlayerReward) && hunt.Nemesis.GetReference() == None && hunt.StolenWeapon.GetReference() == None && Game.IsMovementControlsEnabled() && Game.IsFightingControlsEnabled() && Game.IsLookingControlsEnabled())
EndFunction
