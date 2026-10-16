Scriptname OSFDefeatAcceptance extends ObjectReference

Actor npc
Int playerDownToken
Int playerUpToken
Int consequenceToken
Int imminentToken
Int actorDownToken
Int actorUpToken
String packKind

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
