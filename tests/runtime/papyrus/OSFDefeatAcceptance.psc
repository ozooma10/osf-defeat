Scriptname OSFDefeatAcceptance extends ObjectReference

Actor npc
Int playerDownToken
Int playerUpToken
Int consequenceToken
Int imminentToken
Int actorDownToken
Int actorUpToken
String packKind
OSFDefeatShadowsHunt hunt
Actor shadow
Weapon testWeapon
ActorValue resistance
Float playerResistance
Float shadowResistance
Float defeatDay
Int shadowCredits

Actor Function NewShadow()
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
            OSFDefeatAcceptanceProbe.Record("shadow-candidate-level", True, candidate, candidate.GetLevel())
            If candidate.GetLevel() >= Game.GetPlayer().GetLevel() && hunt.Eligible(candidate)
                candidate.Enable()
                candidate.MoveTo(Game.GetPlayer(), 8.0, 0.0, 0.0)
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

Function ShadowsPrepare()
    Actor player = Game.GetPlayer()
    hunt = Game.GetFormFromFile(0x801, "OSFDefeatShadows.esm") as OSFDefeatShadowsHunt
    If hunt == None
        OSFDefeatAcceptanceProbe.Record("shadows-ready", False)
        Return
    EndIf
    hunt.Start()
    Utility.Wait(0.5)
    shadow = NewShadow()
    If shadow == None
        OSFDefeatAcceptanceProbe.Record("shadows-ready", False, player, player.GetLevel())
        Return
    EndIf
    testWeapon = Game.GetFormFromFile(0x476C4, "Starfield.esm") as Weapon
    player.RemoveItem(testWeapon, player.GetItemCount(testWeapon), True)
    player.AddItem(testWeapon, 1, True)
    player.EquipItem(testWeapon, False, True)
    Weapon old = shadow.GetEquippedWeapon()
    If old != None
        shadow.UnequipItem(old, True, True)
        shadow.RemoveItem(old, shadow.GetItemCount(old), True)
    EndIf
    MiscObject credits = Game.GetCredits()
    player.RemoveItem(credits, player.GetItemCount(credits), True)
    player.AddItem(credits, 1000, True)
    shadowCredits = shadow.GetItemCount(credits)
    resistance = Game.GetFormFromFile(0x2E3, "Starfield.esm") as ActorValue
    playerResistance = player.GetValue(resistance)
    shadowResistance = shadow.GetValue(resistance)
    defeatDay = Utility.GetCurrentGameTime()
    OSFDefeatAcceptanceProbe.Record("shadows-ready", player.GetEquippedWeapon() == testWeapon && !OSFDefeat.HasQuestAlias(shadow) && !OSFDefeat.HasProtectionOverride(shadow), shadow)
EndFunction

Function ShadowsInspect()
    Actor player = Game.GetPlayer()
    ; Let the actual NPC process its newly added ability before reading actor values.
    ; The active truce is what must keep this hostile actor from attacking now.
    Float healthBefore = player.GetValue(Game.GetFormFromFile(0x2D4, "Starfield.esm") as ActorValue)
    OSFDefeat.ResumeActorAI(shadow)
    Utility.Wait(2.0)
    ObjectReference stolen = hunt.StolenWeapon.GetReference()
    Bool ok = hunt.Nemesis.GetActorReference() == shadow && OSFDefeat.HasQuestAlias(shadow)
    OSFDefeatAcceptanceProbe.Record("shadows-alias", ok, shadow)
    OSFDefeatAcceptanceProbe.Record("shadows-objective", hunt.IsObjectiveDisplayed(10), shadow)
    OSFDefeatAcceptanceProbe.Record("shadows-buff", shadow.HasSpell(hunt.NemesisBuff), shadow)
    OSFDefeatAcceptanceProbe.Record("shadows-player-credits", player.GetItemCount(Game.GetCredits()) == 900, player, player.GetItemCount(Game.GetCredits()))
    OSFDefeatAcceptanceProbe.Record("shadows-nemesis-credits", shadow.GetItemCount(Game.GetCredits()) == shadowCredits + 100, shadow, shadow.GetItemCount(Game.GetCredits()) - shadowCredits)
    OSFDefeatAcceptanceProbe.Record("shadows-weapon-ref", stolen != None, shadow)
    OSFDefeatAcceptanceProbe.Record("shadows-theft-status", hunt.LastTheftStatus == 7, shadow, hunt.LastTheftStatus)
    OSFDefeatAcceptanceProbe.Record("shadows-theft-source", hunt.LastTheftSource == 0x14, player, hunt.LastTheftSource)
    If hunt.LastWeaponConsidered != None
        OSFDefeatAcceptanceProbe.Record("shadows-considered-weapon", hunt.LastWeaponConsidered == testWeapon, player, hunt.LastWeaponConsidered.GetFormID())
    EndIf
    OSFDefeatAcceptanceProbe.Record("shadows-equipped-weapon", shadow.GetEquippedWeapon() == testWeapon, shadow)
    If stolen != None
        OSFDefeatAcceptanceProbe.Record("shadows-weapon-container", stolen.GetContainer() == shadow, shadow)
    EndIf
    OSFDefeatAcceptanceProbe.Record("shadows-player-weapon", player.GetItemCount(testWeapon) == 0, player, player.GetItemCount(testWeapon))
    OSFDefeatAcceptanceProbe.Record("shadows-nemesis-weapon", shadow.GetItemCount(testWeapon) == 1, shadow, shadow.GetItemCount(testWeapon))
    OSFDefeatAcceptanceProbe.Record("shadows-time", Utility.GetCurrentGameTime() - defeatDay >= 0.083, player, (Utility.GetCurrentGameTime() - defeatDay) * 24.0)
    OSFDefeatAcceptanceProbe.Record("shadows-controls", Game.IsMovementControlsEnabled() && Game.IsFightingControlsEnabled())
    OSFDefeatAcceptanceProbe.Record("shadows-truce-live-ai", OSFDefeat.IsRecoveryTruceActive(player) && player.GetValue(Game.GetFormFromFile(0x2D4, "Starfield.esm") as ActorValue) >= healthBefore, player)
    ok = ok && hunt.IsObjectiveDisplayed(10) && shadow.HasSpell(hunt.NemesisBuff) && player.HasSpell(hunt.PlayerDebuff)
    ok = ok && player.GetItemCount(Game.GetCredits()) == 900 && shadow.GetItemCount(Game.GetCredits()) == shadowCredits + 100
    ok = ok && stolen != None && stolen.GetContainer() == shadow && player.GetItemCount(testWeapon) == 0 && shadow.GetItemCount(testWeapon) == 1
    ok = ok && shadow.GetEquippedWeapon() == testWeapon
    ok = ok && OSFDefeat.IsRecoveryTruceActive(player) && player.GetValue(Game.GetFormFromFile(0x2D4, "Starfield.esm") as ActorValue) >= healthBefore
    ok = ok && Utility.GetCurrentGameTime() - defeatDay >= 0.083
    ok = ok && Game.IsMovementControlsEnabled() && Game.IsFightingControlsEnabled()
    OSFDefeatAcceptanceProbe.Record("shadows-aftermath", ok, shadow)
    OSFDefeatAcceptanceProbe.Record("shadows-player-resistance", Math.Abs(player.GetValue(resistance) - (playerResistance - 20.0)) < 0.1, player, player.GetValue(resistance) - playerResistance)
    OSFDefeatAcceptanceProbe.Record("shadows-nemesis-resistance", Math.Abs(shadow.GetValue(resistance) - (shadowResistance + 40.0)) < 0.1, shadow, shadow.GetValue(resistance) - shadowResistance)
    Bool replaced = hunt.Promote(npc)
    OSFDefeatAcceptanceProbe.Record("shadows-slot-preserved", !replaced && hunt.Nemesis.GetActorReference() == shadow && player.HasSpell(hunt.PlayerDebuff), shadow)
    OSFDefeat.SuspendActorAI(shadow)
    ; Leave the player unarmed so the next real mouse click tests a missed punch.
    Weapon remaining = player.GetEquippedWeapon()
    If remaining != None
        player.UnequipItem(remaining, False, True)
    EndIf
    player.DrawWeapon()
EndFunction

Function ShadowsPlayerKill()
    shadow.Kill(Game.GetPlayer())
    Utility.Wait(2.0)
    Bool ok = !Game.GetPlayer().HasSpell(hunt.PlayerDebuff) && Game.GetPlayer().HasSpell(hunt.PlayerReward)
    ok = ok && hunt.Nemesis.GetReference() == None && hunt.IsObjectiveCompleted(10)
    ok = ok && shadow.GetItemCount(testWeapon) == 1 && shadow.GetItemCount(Game.GetCredits()) == shadowCredits + 100
    OSFDefeatAcceptanceProbe.Record("shadows-player-reward", ok, shadow)
    Game.GetPlayer().RemoveSpell(hunt.PlayerReward)
    shadow = NewShadow()
    If shadow == None
        OSFDefeatAcceptanceProbe.Record("shadows-other-kill", False)
        Return
    EndIf
    Bool promoted = hunt.Promote(shadow)
    shadow.Kill(npc)
    Utility.Wait(2.0)
    ok = promoted && !Game.GetPlayer().HasSpell(hunt.PlayerDebuff) && !Game.GetPlayer().HasSpell(hunt.PlayerReward)
    ok = ok && hunt.Nemesis.GetReference() == None
    OSFDefeatAcceptanceProbe.Record("shadows-other-kill", ok, shadow)
EndFunction

Function Initialize()
    ; A fresh persistent NPC reference, using the fixture player's valid actor base.
    ; This avoids modifying an existing quest NPC or depending on a workstation FormID.
    Actor player = Game.GetPlayer()
    npc = player.PlaceAtMe(player.GetBaseObject(), 1, True, False, False, abSnapOffsetToNavmesh = False) as Actor
    If npc != None
        npc.MoveTo(player, 5.0, 0.0, 0.0)
    EndIf
    OSFDefeatAcceptanceProbe.Record("fixture", npc != None && npc != player, npc)
    RegisterCallbacks()
EndFunction

Function PreparePack(String kind)
    packKind = kind
    Actor player = Game.GetPlayer()
    Keyword robot = Game.GetFormFromFile(0x002702C9, "Starfield.esm") as Keyword
    Keyword creature = Game.GetFormFromFile(0x001469CF, "Starfield.esm") as Keyword
    Faction varuun = Game.GetFormFromFile(0x0004CE29, "Starfield.esm") as Faction
    Faction security = Game.GetFormFromFile(0x0015983F, "Starfield.esm") as Faction
    npc.RemoveKeyword(robot)
    npc.RemoveKeyword(creature)
    npc.RemoveFromFaction(varuun)
    npc.RemoveFromFaction(security)
    npc.SetCrimeFaction(None)
    If kind == "message"
        npc.AddToFaction(varuun)
    ElseIf kind == "scrapped"
        npc.AddKeyword(robot)
    ElseIf kind == "mauled"
        npc.AddKeyword(creature)
    ElseIf kind == "arrest-fallback"
        npc.AddToFaction(security)
    EndIf
    MiscObject credits = Game.GetCredits()
    MiscObject picks = Game.GetFormFromFile(0xA, "Starfield.esm") as MiscObject
    player.RemoveItem(credits, player.GetItemCount(credits), True)
    npc.RemoveItem(credits, npc.GetItemCount(credits), True)
    player.AddItem(credits, 1000, True)
    player.RemoveItem(picks, player.GetItemCount(picks), True)
    npc.RemoveItem(picks, npc.GetItemCount(picks), True)
    player.AddItem(picks, 4, True)
    player.RemoveSpell(Game.GetFormFromFile(0x2BDD1B, "Starfield.esm") as Spell)
    player.RemoveSpell(Game.GetFormFromFile(0x2BDD1A, "Starfield.esm") as Spell)
    player.RemoveSpell(Game.GetFormFromFile(0x2BDD19, "Starfield.esm") as Spell)
    player.RemoveSpell(Game.GetFormFromFile(0x2BDD0C, "Starfield.esm") as Spell)
    OSFDefeatAcceptanceProbe.Record("pack-prepared-" + kind, npc != None && player.GetItemCount(credits) == 1000 && npc.GetCrimeFaction() == None)
EndFunction

Function InspectPack()
    Actor player = Game.GetPlayer()
    MiscObject credits = Game.GetCredits()
    MiscObject picks = Game.GetFormFromFile(0xA, "Starfield.esm") as MiscObject
    Bool ok = !OSFDefeat.IsDefeated(player) && Game.IsMovementControlsEnabled() && Game.IsFightingControlsEnabled()
    Int questId = 0
    If packKind == "robbed"
        ok = ok && player.GetItemCount(credits) == 900 && npc.GetItemCount(credits) == 100
        questId = 0x800
    ElseIf packKind == "picked-clean"
        ok = ok && player.GetItemCount(credits) == 750 && npc.GetItemCount(credits) == 250
        ok = ok && player.GetItemCount(picks) == 0 && npc.GetItemCount(picks) == 4
        ok = ok && player.HasSpell(Game.GetFormFromFile(0x2BDD1B, "Starfield.esm"))
        questId = 0x803
    ElseIf packKind == "message"
        ok = ok && player.GetItemCount(credits) == 1000 && player.HasSpell(Game.GetFormFromFile(0x2BDD1A, "Starfield.esm"))
        questId = 0x804
    ElseIf packKind == "scrapped"
        ok = ok && player.HasSpell(Game.GetFormFromFile(0x2BDD19, "Starfield.esm"))
        questId = 0x805
    ElseIf packKind == "mauled"
        ok = ok && player.HasSpell(Game.GetFormFromFile(0x2BDD0C, "Starfield.esm"))
        questId = 0x806
    ElseIf packKind == "arrest-fallback"
        ok = ok && npc.GetCrimeFaction() == None && player.GetItemCount(credits) == 1000
        questId = 0x801
    EndIf
    Quest consequence = Game.GetFormFromFile(questId, "OSFDefeatBasic.esm") as Quest
    ok = ok && consequence != None && !consequence.IsRunning()
    OSFDefeatAcceptanceProbe.Record("pack-effect-" + packKind, ok, npc, player.GetItemCount(credits))
EndFunction

Function MarkerRecovery()
    Actor player = Game.GetPlayer()
    Form markerBase = Game.GetFormFromFile(0x3B, "Starfield.esm")
    Keyword recovery = Game.GetFormFromFile(0x807, "OSFDefeatBasic.esm") as Keyword
    ObjectReference marker = player.PlaceAtMe(markerBase, 1, True, False, False, abSnapOffsetToNavmesh = False)
    marker.MoveTo(player, 3.0, 0.0, 0.0)
    marker.AddKeyword(recovery)
    Float before = player.GetDistance(marker)
    OSFDefeatBasicRecovery.FinishAftermath(player)
    OSFDefeatAcceptanceProbe.Record("marker-recovery", before > 2.0 && player.GetDistance(marker) < 1.0 && !OSFDefeat.IsDefeated(player), player, player.GetDistance(marker))
    marker.Disable()
    marker.Delete()
EndFunction

Function RegisterCallbacks()
    playerDownToken = OSFDefeat.RegisterForPlayerDefeated(Self, "PlayerDown")
    playerUpToken = OSFDefeat.RegisterForPlayerRecovered(Self, "PlayerUp")
    consequenceToken = OSFDefeat.RegisterForPlayerConsequenceStarted(Self, "Consequence")
    imminentToken = OSFDefeat.RegisterForPlayerDefeatImminent(Self, "Imminent")
    actorDownToken = OSFDefeat.RegisterForActorDefeated(Self, "ActorDown")
    actorUpToken = OSFDefeat.RegisterForActorRecovered(Self, "ActorUp")
    Bool ok = playerDownToken != 0 && playerUpToken != 0 && consequenceToken != 0 && imminentToken != 0 && actorDownToken != 0 && actorUpToken != 0
    OSFDefeatAcceptanceProbe.Record("registered", ok)
EndFunction

Function UnregisterCallbacks()
    Bool ok = OSFDefeat.UnregisterForPlayerDefeated(playerDownToken)
    ok = OSFDefeat.UnregisterForPlayerRecovered(playerUpToken) && ok
    ok = OSFDefeat.UnregisterForPlayerConsequenceStarted(consequenceToken) && ok
    ok = OSFDefeat.UnregisterForPlayerDefeatImminent(imminentToken) && ok
    ok = OSFDefeat.UnregisterForActorDefeated(actorDownToken) && ok
    ok = OSFDefeat.UnregisterForActorRecovered(actorUpToken) && ok
    OSFDefeatAcceptanceProbe.Record("unregistered", ok)
EndFunction

Function PlayerDown()
    OSFDefeatAcceptanceProbe.Record("event-player-down", OSFDefeat.IsDefeated(Game.GetPlayer()))
EndFunction
Function PlayerUp()
    OSFDefeatAcceptanceProbe.Record("event-player-up", !OSFDefeat.IsDefeated(Game.GetPlayer()))
EndFunction
Function Consequence()
    OSFDefeatAcceptanceProbe.Record("event-consequence", OSFDefeat.GetLastPlayerConsequenceId() != "")
EndFunction
Function Imminent()
    OSFDefeatAcceptanceProbe.Record("event-imminent", !OSFDefeat.IsDefeated(Game.GetPlayer()))
EndFunction
Function ActorDown(Actor victim, Actor aggressor)
    OSFDefeatAcceptanceProbe.Record("event-actor-down", victim != None, victim)
EndFunction
Function ActorUp(Actor victim, Actor aggressor)
    OSFDefeatAcceptanceProbe.Record("event-actor-up", victim != None, victim)
EndFunction

Function InspectDownControls()
    OSFDefeatAcceptanceProbe.Record("controls-down", !Game.IsMovementControlsEnabled() && !Game.IsFightingControlsEnabled() && Game.IsLookingControlsEnabled())
EndFunction
Function InspectRecoveredControls()
    OSFDefeatAcceptanceProbe.Record("controls-up", Game.IsMovementControlsEnabled() && Game.IsFightingControlsEnabled() && Game.IsLookingControlsEnabled())
EndFunction
Function RebuildNpc()
    npc.Disable()
    Utility.Wait(0.5)
    npc.Enable()
    Float waited = 0.0
    While !npc.Is3DLoaded() && waited < 10.0
        Utility.Wait(0.1)
        waited += 0.1
    EndWhile
    ; Deliberately separate the captive from the destination before testing transfer.
    npc.MoveTo(Game.GetPlayer(), 5.0, 0.0, 0.0)
    OSFDefeatAcceptanceProbe.Record("npc-rebuilt", npc.Is3DLoaded(), npc)
EndFunction
