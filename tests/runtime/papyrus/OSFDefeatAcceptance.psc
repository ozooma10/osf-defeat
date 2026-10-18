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
Actor repeatAggressor
Int repeatCredits
Int repeatAggressorCredits
Int throwableBefore
Float checkpointDay
Int checkpointPhase

Actor Function NewShadow(Bool lowerLevel = False)
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
    If lowerLevel
        ; Installed ESM EncCrimsonFleet01Template is level 2; Spacer starts at 8.
        bases = new Int[1]
        bases[0] = 0x10B2E
    EndIf
    Int i = 0
    While i < bases.Length
        Actor candidate = Game.GetPlayer().PlaceAtMe(Game.GetFormFromFile(bases[i], "Starfield.esm"), 1, False, True, False) as Actor
        If candidate != None
            OSFDefeatAcceptanceProbe.Record("shadow-candidate-level", True, candidate, candidate.GetLevel())
            Bool levelMatch = candidate.GetLevel() >= Game.GetPlayer().GetLevel()
            If lowerLevel
                levelMatch = candidate.GetLevel() < Game.GetPlayer().GetLevel()
            EndIf
            If levelMatch && hunt.Eligible(candidate)
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
    PrepareShadowsFixture(True)
EndFunction

Function PrepareShadowsFixture(Bool testEligibility)
    Actor player = Game.GetPlayer()
    hunt = Game.GetFormFromFile(0x801, "OSFDefeatShadows.esm") as OSFDefeatShadowsHunt
    If hunt == None
        OSFDefeatAcceptanceProbe.Record("shadows-ready", False)
        Return
    EndIf
    hunt.Start()
    Utility.Wait(0.5)
    If testEligibility
        ShadowsNegativeEligibility()
    EndIf
    shadow = NewShadow()
    If shadow == None
        OSFDefeatAcceptanceProbe.Record("shadows-ready", False, player, player.GetLevel())
        Return
    EndIf
    testWeapon = Game.GetFormFromFile(0x476C4, "Starfield.esm") as Weapon
    player.RemoveItem(testWeapon, player.GetItemCount(testWeapon), True)
    player.AddItem(testWeapon, 1, True)
    ; Physical suppressor plus all three legendary attachment slots. Verify the
    ; actual instance array, not just successful Papyrus return values or a name.
    player.AttachModToInventoryItem(testWeapon, Game.GetFormFromFile(0x14127D, "Starfield.esm") as ObjectMod)
    player.AttachModToInventoryItem(testWeapon, Game.GetFormFromFile(0xF2013, "Starfield.esm") as ObjectMod)
    player.AttachModToInventoryItem(testWeapon, Game.GetFormFromFile(0xFEA04, "Starfield.esm") as ObjectMod)
    player.AttachModToInventoryItem(testWeapon, Game.GetFormFromFile(0xFA8D6, "Starfield.esm") as ObjectMod)
    player.EquipItem(testWeapon, False, True)
    OSFDefeatAcceptanceProbe.VerifyWeapon(player, testWeapon, "baseline")
    Weapon old = shadow.GetEquippedWeapon()
    If old != None
        shadow.UnequipItem(old, True, True)
        shadow.RemoveItem(old, shadow.GetItemCount(old), True)
    EndIf
    Ammo ammunition = testWeapon.GetAmmo()
    shadow.RemoveItem(ammunition, shadow.GetItemCount(ammunition), True)
    OSFDefeatAcceptanceProbe.Record("shadows-no-ammo-fixture", shadow.GetItemCount(ammunition) == 0, shadow)
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
    Int equipWait = 0
    While shadow.GetEquippedWeapon() != testWeapon && equipWait < 30
        Utility.Wait(0.1)
        equipWait += 1
    EndWhile
    Weapon actualEquipped = shadow.GetEquippedWeapon()
    If actualEquipped != None
        OSFDefeatAcceptanceProbe.Record("shadows-actual-equipped", True, shadow, actualEquipped.GetFormID())
    EndIf
    OSFDefeatAcceptanceProbe.Record("shadows-ammo-count", shadow.GetItemCount(testWeapon.GetAmmo()) == 1, shadow, shadow.GetItemCount(testWeapon.GetAmmo()))
    ObjectReference stolen = hunt.StolenWeapon.GetReference()
    OSFDefeatAcceptanceProbe.VerifyWeapon(shadow, testWeapon, "transferred")
    Bool ok = hunt.Nemesis.GetActorReference() == shadow && OSFDefeat.HasQuestAlias(shadow)
    OSFDefeatAcceptanceProbe.Record("shadows-alias", ok, shadow)
    OSFDefeatAcceptanceProbe.Record("shadows-objective", hunt.IsObjectiveDisplayed(10), shadow)
    OSFDefeatAcceptanceProbe.Record("shadows-buff", shadow.HasSpell(hunt.NemesisBuff), shadow)
    OSFDefeatAcceptanceProbe.Record("shadows-player-credits", player.GetItemCount(Game.GetCredits()) == 900, player, player.GetItemCount(Game.GetCredits()))
    OSFDefeatAcceptanceProbe.Record("shadows-nemesis-credits", shadow.GetItemCount(Game.GetCredits()) == shadowCredits + 100, shadow, shadow.GetItemCount(Game.GetCredits()) - shadowCredits)
    OSFDefeatAcceptanceProbe.Record("shadows-weapon-ref", stolen != None, shadow)
    OSFDefeatAcceptanceProbe.Record("shadows-theft-status", hunt.LastTheftStatus == 7, shadow, hunt.LastTheftStatus)
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
    ok = ok && stolen != None && stolen.GetBaseObject() == testWeapon && player.GetItemCount(testWeapon) == 0 && shadow.GetItemCount(testWeapon) == 1
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
    ObjectReference stolen = hunt.StolenWeapon.GetReference()
    ; Give the engine a live, drawn weapon at death. Killing a suspended actor
    ; can leave it in corpse inventory without exercising the natural drop path.
    OSFDefeat.ResumeActorAI(shadow)
    shadow.DrawWeapon()
    Int drawnWait = 0
    While !shadow.IsWeaponDrawn() && drawnWait < 30
        Utility.Wait(0.1)
        drawnWait += 1
    EndWhile
    OSFDefeatAcceptanceProbe.Record("shadows-death-weapon-drawn", shadow.IsWeaponDrawn() && shadow.GetEquippedWeapon() == testWeapon, shadow)
    shadow.Kill(Game.GetPlayer())
    ; Death completion, like ability updates, needs the actor's process running.
    OSFDefeat.ResumeActorAI(shadow)
    Int waited = 0
    While hunt.Nemesis.GetActorReference() == shadow && waited < 50
        Utility.Wait(0.1)
        waited += 1
    EndWhile
    OSFDefeatAcceptanceProbe.Record("shadows-player-kill-dead", shadow.IsDead(), shadow)
    OSFDefeatAcceptanceProbe.Record("shadows-player-kill-debuff", !Game.GetPlayer().HasSpell(hunt.PlayerDebuff), shadow)
    OSFDefeatAcceptanceProbe.Record("shadows-player-kill-buff", Game.GetPlayer().HasSpell(hunt.PlayerReward), shadow)
    OSFDefeatAcceptanceProbe.Record("shadows-player-kill-alias", hunt.Nemesis.GetReference() == None, shadow)
    OSFDefeatAcceptanceProbe.Record("shadows-player-kill-loot", shadow.GetItemCount(testWeapon) == 1, shadow, shadow.GetItemCount(testWeapon))
    ObjectReference dropped = Game.FindClosestReferenceOfTypeFromRef(testWeapon, shadow, 5.0)
    OSFDefeatAcceptanceProbe.Record("shadows-dropped-weapon", dropped != None, shadow)
    OSFDefeatAcceptanceProbe.Record("shadows-dropped-identity", dropped != None && dropped == stolen, shadow)
    If shadow.GetItemCount(testWeapon) == 1
        OSFDefeatAcceptanceProbe.Record("shadows-retrieval-corpse", True)
        shadow.RemoveItem(testWeapon, 1, True, Game.GetPlayer())
    ElseIf dropped != None && OSFDefeatAcceptanceProbe.VerifyDroppedWeapon(dropped, testWeapon)
        ; Death can materialize a different reference from the inventory alias.
        ; Require the exact modified instance data before accepting that drop.
        OSFDefeatAcceptanceProbe.Record("shadows-retrieval-natural-drop", True)
        dropped.Activate(Game.GetPlayer())
    EndIf
    Utility.Wait(0.5)
    Bool recovered = Game.GetPlayer().GetItemCount(testWeapon) == 1
    OSFDefeatAcceptanceProbe.VerifyWeapon(Game.GetPlayer(), testWeapon, "retrieved")
    ; Exercise a real world reference separately from the corpse-loot branch.
    ; This forces DropObject; it does not claim the NPC naturally dropped it.
    ObjectReference worldWeapon = Game.GetPlayer().DropObject(testWeapon, 1)
    Utility.Wait(0.5)
    Bool leftInventory = worldWeapon != None && Game.GetPlayer().GetItemCount(testWeapon) == 0
    If worldWeapon != None
        worldWeapon.Activate(Game.GetPlayer())
    EndIf
    Utility.Wait(0.5)
    OSFDefeatAcceptanceProbe.Record("shadows-world-drop", leftInventory && Game.GetPlayer().GetItemCount(testWeapon) == 1)
    OSFDefeatAcceptanceProbe.VerifyWeapon(Game.GetPlayer(), testWeapon, "world-pickup")
    OSFDefeatAcceptanceProbe.Record("shadows-weapon-retrieved", recovered, Game.GetPlayer(), Game.GetPlayer().GetItemCount(testWeapon))
    Bool ok = !Game.GetPlayer().HasSpell(hunt.PlayerDebuff) && Game.GetPlayer().HasSpell(hunt.PlayerReward)
    ok = ok && hunt.Nemesis.GetReference() == None && hunt.IsObjectiveCompleted(10)
    ok = ok && recovered && shadow.GetItemCount(Game.GetCredits()) == shadowCredits + 100
    OSFDefeatAcceptanceProbe.Record("shadows-player-reward", ok, shadow)
    Game.GetPlayer().RemoveSpell(hunt.PlayerReward)
    shadow = NewShadow()
    If shadow == None
        OSFDefeatAcceptanceProbe.Record("shadows-other-kill", False)
        Return
    EndIf
    Bool promoted = hunt.Promote(shadow)
    shadow.Kill(npc)
    OSFDefeat.ResumeActorAI(shadow)
    waited = 0
    While hunt.Nemesis.GetActorReference() == shadow && waited < 50
        Utility.Wait(0.1)
        waited += 1
    EndWhile
    OSFDefeatAcceptanceProbe.Record("shadows-other-kill-promoted", promoted, shadow)
    OSFDefeatAcceptanceProbe.Record("shadows-other-kill-dead", shadow.IsDead(), shadow)
    ok = promoted && !Game.GetPlayer().HasSpell(hunt.PlayerDebuff) && !Game.GetPlayer().HasSpell(hunt.PlayerReward)
    ok = ok && hunt.Nemesis.GetReference() == None
    OSFDefeatAcceptanceProbe.Record("shadows-other-kill", ok, shadow)
    shadow = NewShadow(True)
    If shadow == None
        OSFDefeatAcceptanceProbe.Record("shadows-lower-level-no-reward", False)
        Return
    EndIf
    promoted = hunt.Promote(shadow)
    shadow.Kill(Game.GetPlayer())
    OSFDefeat.ResumeActorAI(shadow)
    waited = 0
    While hunt.Nemesis.GetActorReference() == shadow && waited < 50
        Utility.Wait(0.1)
        waited += 1
    EndWhile
    OSFDefeatAcceptanceProbe.Record("shadows-lower-level-promoted", promoted, shadow)
    OSFDefeatAcceptanceProbe.Record("shadows-lower-level-dead", shadow.IsDead(), shadow)
    OSFDefeatAcceptanceProbe.Record("shadows-lower-level-alias", hunt.Nemesis.GetReference() == None, shadow)
    OSFDefeatAcceptanceProbe.Record("shadows-lower-level-reward-absent", !Game.GetPlayer().HasSpell(hunt.PlayerReward), shadow)
    OSFDefeatAcceptanceProbe.Record("shadows-lower-level-debuff-absent", !Game.GetPlayer().HasSpell(hunt.PlayerDebuff), shadow)
    OSFDefeatAcceptanceProbe.Record("shadows-lower-level-no-reward", promoted && shadow.IsDead() && hunt.Nemesis.GetReference() == None && !Game.GetPlayer().HasSpell(hunt.PlayerReward) && !Game.GetPlayer().HasSpell(hunt.PlayerDebuff), shadow, shadow.GetLevel())
EndFunction

Function PrepareRepeatDefeat()
    repeatCredits = Game.GetPlayer().GetItemCount(Game.GetCredits())
    repeatAggressor = NewShadow()
    OSFDefeatAcceptanceProbe.Record("shadows-repeat-ready", repeatAggressor != None, repeatAggressor)
EndFunction

Function InspectRepeatDefeat()
    Bool ok = hunt.Nemesis.GetActorReference() == shadow && Game.GetPlayer().HasSpell(hunt.PlayerDebuff)
    ok = ok && Game.GetPlayer().GetItemCount(Game.GetCredits()) == repeatCredits && shadow.GetItemCount(testWeapon) == 1
    ok = ok && !OSFDefeat.IsDefeated(Game.GetPlayer()) && OSFDefeat.IsRecoveryTruceActive(Game.GetPlayer())
    OSFDefeatAcceptanceProbe.Record("shadows-repeat-defeat", ok, shadow)
    OSFDefeat.ResumeActorAI(repeatAggressor)
    repeatAggressor.Disable()
    repeatAggressor.Delete()
EndFunction

Function PrepareBlackout()
    hunt.CancelHunt()
    repeatAggressor = NewShadow()
    repeatCredits = Game.GetPlayer().GetItemCount(Game.GetCredits())
    If repeatAggressor != None
        repeatAggressorCredits = repeatAggressor.GetItemCount(Game.GetCredits())
    EndIf
    defeatDay = Utility.GetCurrentGameTime()
    OSFDefeatAcceptanceProbe.Record("shadows-repeat-ready", repeatAggressor != None, repeatAggressor)
EndFunction

Function PrepareCheckpoint()
    OSFDefeat.EndRecoveryTruce(Game.GetPlayer())
    If hunt != None
        hunt.CancelHunt()
    EndIf
    ; PCF0 persists AI suspensions. Release the previous disposable subjects
    ; before seeding this checkpoint so its sidecar contains only this actor.
    If shadow != None
        OSFDefeat.ResumeActorAI(shadow)
        shadow.Disable()
        shadow.Delete()
    EndIf
    If repeatAggressor != None
        OSFDefeat.ResumeActorAI(repeatAggressor)
        repeatAggressor.Disable()
        repeatAggressor.Delete()
        repeatAggressor = None
    EndIf
    Utility.Wait(0.5)
    PrepareShadowsFixture(False)
EndFunction

Function ObserveCheckpoint(String kind)
    OSFDefeatShadowsAftermath aftermath = Game.GetFormFromFile(0x800, "OSFDefeatShadows.esm") as OSFDefeatShadowsAftermath
    checkpointPhase = 2
    If kind == "time"
        checkpointPhase = 3
    EndIf
    Int waited = 0
    While aftermath.Phase != checkpointPhase && waited < 150
        Utility.Wait(0.1)
        waited += 1
    EndWhile
    checkpointDay = Utility.GetCurrentGameTime()
    Float hours = (checkpointDay - defeatDay) * 24.0
    Bool timeOK = hours < 0.1
    If kind == "time"
        timeOK = hours >= 2.0 && hours < 3.1
    EndIf
    Bool ok = aftermath.Phase == checkpointPhase && aftermath.IsRunning() && OSFDefeat.IsDefeated(Game.GetPlayer())
    ok = ok && hunt.Nemesis.GetActorReference() == shadow && Game.GetPlayer().GetItemCount(Game.GetCredits()) == 900
    ok = ok && shadow.GetItemCount(Game.GetCredits()) == shadowCredits + 100 && hunt.LastTheftStatus == 7
    ok = ok && OSFDefeatAcceptanceProbe.VerifyWeapon(shadow, testWeapon, "checkpoint-" + kind)
    OSFDefeatAcceptanceProbe.Record("shadows-blackout-" + kind + "-hours", timeOK, None, hours)
    OSFDefeatAcceptanceProbe.Record("shadows-blackout-" + kind + "-ready", ok && timeOK, shadow, aftermath.Phase)
EndFunction

Function InspectLoadedCheckpoint(String kind)
    ; Sample twice so a saved latent stack cannot pass before replaying later.
    Int sample = 0
    Bool ok = (kind == "theft" && checkpointPhase == 2) || (kind == "time" && checkpointPhase == 3)
    While sample < 2
        Utility.Wait(3.0)
        Actor player = Game.GetPlayer()
        OSFDefeatShadowsAftermath aftermath = Game.GetFormFromFile(0x800, "OSFDefeatShadows.esm") as OSFDefeatShadowsAftermath
        ObjectReference stolen = hunt.StolenWeapon.GetReference()
        Bool creditsOK = player.GetItemCount(Game.GetCredits()) == 900 && shadow.GetItemCount(Game.GetCredits()) == shadowCredits + 100
        Bool huntOK = hunt.Nemesis.GetActorReference() == shadow && hunt.IsObjectiveDisplayed(10)
        huntOK = huntOK && player.HasSpell(hunt.PlayerDebuff) && shadow.HasSpell(hunt.NemesisBuff)
        ; MakeAliasedRefFromInventory proxies can have no GetContainer even
        ; while the real instance is owned. Inspect actual inventory OMODs.
        Bool modsOK = OSFDefeatAcceptanceProbe.VerifyWeapon(shadow, testWeapon, "loaded-" + kind)
        Bool weaponOK = stolen != None && stolen.GetBaseObject() == testWeapon
        weaponOK = weaponOK && player.GetItemCount(testWeapon) == 0 && shadow.GetItemCount(testWeapon) == 1
        weaponOK = weaponOK && modsOK
        OSFDefeatAcceptanceProbe.Record("checkpoint-weapon-alias", stolen != None && stolen.GetBaseObject() == testWeapon)
        OSFDefeatAcceptanceProbe.Record("checkpoint-player-weapon-count", player.GetItemCount(testWeapon) == 0, player, player.GetItemCount(testWeapon))
        OSFDefeatAcceptanceProbe.Record("checkpoint-nemesis-weapon-count", shadow.GetItemCount(testWeapon) == 1, shadow, shadow.GetItemCount(testWeapon))
        Bool recovered = !OSFDefeat.IsDefeated(player) && Game.IsMovementControlsEnabled() && Game.IsFightingControlsEnabled() && Game.IsLookingControlsEnabled()
        Bool stopped = aftermath.LastResult == "loaded-blackout" && !aftermath.IsRunning()
        Float extraHours = (Utility.GetCurrentGameTime() - checkpointDay) * 24.0
        Bool timeOK = extraHours >= 0.0 && extraHours < 0.1
        OSFDefeatAcceptanceProbe.Record("checkpoint-credits", creditsOK, player, player.GetItemCount(Game.GetCredits()))
        OSFDefeatAcceptanceProbe.Record("checkpoint-hunt", huntOK, shadow)
        OSFDefeatAcceptanceProbe.Record("checkpoint-weapon", weaponOK, shadow)
        OSFDefeatAcceptanceProbe.Record("checkpoint-controls", recovered)
        OSFDefeatAcceptanceProbe.Record("checkpoint-stopped", stopped)
        OSFDefeatAcceptanceProbe.Record("checkpoint-no-time-replay", timeOK, None, extraHours)
        ok = ok && creditsOK && huntOK && weaponOK && recovered && stopped && timeOK
        sample += 1
    EndWhile
    OSFDefeatAcceptanceProbe.Record("shadows-blackout-" + kind + "-loaded", ok, shadow, checkpointPhase)
EndFunction

Function PrepareMissedAttackKind(String kind)
    Actor player = Game.GetPlayer()
    ; Keep the disposable player alive if the deliberately missed grenade hits
    ; a nearby wall. The healthy-save reload restores the original value.
    player.SetValue(Game.GetFormFromFile(0x2D4, "Starfield.esm") as ActorValue, 5000.0)
    If kind == "unarmed"
        player.UnequipAll()
    ElseIf kind == "ranged"
        player.RemoveModFromInventoryItem(testWeapon, Game.GetFormFromFile(0xFA8D6, "Starfield.esm") as ObjectMod)
        player.AddItem(testWeapon.GetAmmo(), 100, True)
        player.EquipItem(testWeapon, False, True)
    ElseIf kind == "grenade"
        Weapon grenade = Game.GetFormFromFile(0x115EF, "Starfield.esm") as Weapon
        player.AddItem(grenade, 2, True)
        player.EquipItem(grenade, False, True)
        throwableBefore = player.GetItemCount(grenade)
    EndIf
    player.DrawWeapon()
    Utility.Wait(1.0)
    hunt.LastAttackEvent = ""
    Weapon equipped = player.GetEquippedWeapon()
    Bool ok = Game.IsFightingControlsEnabled() && OSFDefeat.IsRecoveryTruceActive(player)
    If kind == "unarmed"
        ok = ok && (equipped == None || equipped == Game.GetFormFromFile(0x1F4, "Starfield.esm"))
    ElseIf kind == "ranged"
        ok = ok && equipped == testWeapon
    EndIf
    OSFDefeatAcceptanceProbe.Record("shadows-miss-kind-ready", ok)
EndFunction

Function InspectMissedAttackKind(String kind)
    String observed = hunt.LastAttackEvent
    Bool eventMatches = observed == "HitFrame" || observed == "MeleeAttackStart"
    If kind == "ranged"
        eventMatches = observed == "OnPlayerFireWeapon" || observed == "WeaponFire"
    ElseIf kind == "grenade"
        eventMatches = observed == "WeaponFire" && Game.GetPlayer().GetItemCount(Game.GetFormFromFile(0x115EF, "Starfield.esm")) == throwableBefore - 1
    EndIf
    OSFDefeatAcceptanceProbe.Record("shadows-attack-event-" + observed, True)
    If kind == "grenade"
        Utility.Wait(5.0)
    EndIf
    OSFDefeatAcceptanceProbe.Record("shadows-miss-kind-observed", eventMatches && !OSFDefeat.IsRecoveryTruceActive(Game.GetPlayer()))
EndFunction

Function CooperativeScriptedAttack()
    ; A third-party producer that bypasses the player's graph/fire callbacks
    ; must explicitly end the truce through the public API before its attack.
    Bool queued = OSFDefeat.EndRecoveryTruce(Game.GetPlayer())
    Int waited = 0
    While OSFDefeat.IsRecoveryTruceActive(Game.GetPlayer()) && waited < 20
        Utility.Wait(0.1)
        waited += 1
    EndWhile
    Bool ended = !OSFDefeat.IsRecoveryTruceActive(Game.GetPlayer())
    If queued && ended
        testWeapon.Fire(Game.GetPlayer())
    EndIf
    OSFDefeatAcceptanceProbe.Record("shadows-scripted-attack-contract", queued && ended)
EndFunction

Function WatchBlackout(String mode)
    OSFDefeatShadowsAftermath aftermath = Game.GetFormFromFile(0x800, "OSFDefeatShadows.esm") as OSFDefeatShadowsAftermath
    Int waited = 0
    ; The quest retains the previous run's completed phase until OnQuestInit.
    ; Observe the new blackout before watching any later phase.
    While aftermath.Phase != 1 && waited < 50
        Utility.Wait(0.05)
        waited += 1
    EndWhile
    Bool wasBlack = aftermath.Phase == 1 && OSFDefeat.IsDefeated(Game.GetPlayer())
    If mode == "post-promotion"
        ; Observe real production phases. Do not delay the aftermath to create
        ; a synthetic interruption window or count an already-finished run.
        waited = 0
        While wasBlack && aftermath.Phase < 2 && waited < 1000
            Utility.Wait(0.01)
            waited += 1
        EndWhile
        Int observedPhase = aftermath.Phase
        Bool inProgress = wasBlack && observedPhase >= 2 && observedPhase <= 3 && aftermath.IsRunning()
        OSFDefeatAcceptanceProbe.Record("shadows-blackout-stop-phase", inProgress, repeatAggressor, observedPhase)
        aftermath.Stop()
        Utility.Wait(3.0)
        Actor player = Game.GetPlayer()
        Int taken = (repeatCredits * 0.10) as Int
        If taken < 25
            taken = 25
        ElseIf taken > 2500
            taken = 2500
        EndIf
        If taken > repeatCredits
            taken = repeatCredits
        EndIf
        Bool stopped = inProgress && aftermath.LastResult == "interrupted" && !aftermath.IsRunning()
        Bool recovered = !OSFDefeat.IsDefeated(player) && Game.IsMovementControlsEnabled() && Game.IsFightingControlsEnabled()
        Bool retained = hunt.Nemesis.GetActorReference() == repeatAggressor && hunt.IsObjectiveDisplayed(10)
        retained = retained && player.HasSpell(hunt.PlayerDebuff) && repeatAggressor.HasSpell(hunt.NemesisBuff)
        Bool creditsOnce = player.GetItemCount(Game.GetCredits()) == repeatCredits - taken
        creditsOnce = creditsOnce && repeatAggressor.GetItemCount(Game.GetCredits()) == repeatAggressorCredits + taken
        Float elapsedHours = (Utility.GetCurrentGameTime() - defeatDay) * 24.0
        OSFDefeatAcceptanceProbe.Record("shadows-blackout-promoted-stopped", stopped)
        OSFDefeatAcceptanceProbe.Record("shadows-blackout-promoted-controls", recovered)
        OSFDefeatAcceptanceProbe.Record("shadows-blackout-promoted-hunt", retained, repeatAggressor)
        OSFDefeatAcceptanceProbe.Record("shadows-blackout-promoted-credits", creditsOnce, player, player.GetItemCount(Game.GetCredits()))
        OSFDefeatAcceptanceProbe.Record("shadows-blackout-promoted-hours", elapsedHours < 3.1, player, elapsedHours)
        hunt.CancelHunt()
        OSFDefeat.ResumeActorAI(repeatAggressor)
        repeatAggressor.Disable()
        repeatAggressor.Delete()
        OSFDefeatAcceptanceProbe.Record("shadows-blackout-promoted-interrupted", stopped && recovered && retained && creditsOnce && elapsedHours < 3.1)
        Return
    EndIf
    If mode == "interrupt"
        aftermath.Stop()
        Utility.Wait(3.0)
        Bool ok = wasBlack && aftermath.LastResult == "interrupted" && !aftermath.IsRunning()
        ok = ok && !OSFDefeat.IsDefeated(Game.GetPlayer()) && Game.IsMovementControlsEnabled() && Game.IsFightingControlsEnabled()
        ok = ok && hunt.Nemesis.GetReference() == None && Game.GetPlayer().GetItemCount(Game.GetCredits()) == repeatCredits
        ok = ok && Utility.GetCurrentGameTime() - defeatDay < 0.01
        OSFDefeatAcceptanceProbe.Record("shadows-blackout-interrupted", ok)
        OSFDefeat.ResumeActorAI(repeatAggressor)
        repeatAggressor.Disable()
        repeatAggressor.Delete()
    Else
        OSFDefeatAcceptanceProbe.Record("shadows-blackout-ready", wasBlack)
    EndIf
EndFunction

Function InspectLoadedBlackout()
    Utility.Wait(3.0)
    OSFDefeatShadowsAftermath aftermath = Game.GetFormFromFile(0x800, "OSFDefeatShadows.esm") as OSFDefeatShadowsAftermath
    Bool ok = aftermath.LastResult == "loaded-blackout" && !aftermath.IsRunning()
    OSFDefeatAcceptanceProbe.Record("shadows-loaded-result-" + aftermath.LastResult, ok)
    OSFDefeatAcceptanceProbe.Record("shadows-loaded-credits", Game.GetPlayer().GetItemCount(Game.GetCredits()) == repeatCredits, Game.GetPlayer(), Game.GetPlayer().GetItemCount(Game.GetCredits()))
    OSFDefeatAcceptanceProbe.Record("shadows-loaded-recovered", !OSFDefeat.IsDefeated(Game.GetPlayer()))
    OSFDefeatAcceptanceProbe.Record("shadows-loaded-movement", Game.IsMovementControlsEnabled())
    OSFDefeatAcceptanceProbe.Record("shadows-loaded-fighting", Game.IsFightingControlsEnabled())
    OSFDefeatAcceptanceProbe.Record("shadows-loaded-empty-hunt", hunt.Nemesis.GetReference() == None)
    OSFDefeatAcceptanceProbe.Record("shadows-loaded-time", Utility.GetCurrentGameTime() - defeatDay < 0.01, None, Utility.GetCurrentGameTime() - defeatDay)
    ok = ok && !OSFDefeat.IsDefeated(Game.GetPlayer()) && Game.IsMovementControlsEnabled() && Game.IsFightingControlsEnabled()
    ok = ok && hunt.Nemesis.GetReference() == None && Game.GetPlayer().GetItemCount(Game.GetCredits()) == repeatCredits
    ok = ok && Utility.GetCurrentGameTime() - defeatDay < 0.01
    OSFDefeatAcceptanceProbe.Record("shadows-blackout-loaded", ok)
EndFunction

Function PrepareMissedAttack()
    Actor player = Game.GetPlayer()
    OSFDefeatAcceptanceProbe.VerifyWeapon(shadow, testWeapon, "reloaded")
    ; Equip an explicit melee weapon: after theft, the primary getter can retain
    ; a stale empty gun even after UnequipAll. This also avoids stripping armor.
    Weapon knife = Game.GetFormFromFile(0x35A48, "Starfield.esm") as Weapon
    player.RemoveItem(knife, player.GetItemCount(knife), True)
    player.AddItem(knife, 1, True)
    player.EquipItem(knife, False, True)
    player.DrawWeapon()
    Utility.Wait(1.0)
    Bool registered = RegisterForAnimationEvent(player, "HitFrame")
    Weapon weaponBase = player.GetEquippedWeapon()
    Bool armed = weaponBase == knife && player.GetItemCount(knife) == 1
    OSFDefeatAcceptanceProbe.Record("shadows-miss-weapon", armed, player)
    OSFDefeatAcceptanceProbe.Record("shadows-miss-weapon-count", player.GetItemCount(knife) == 1, player, player.GetItemCount(knife))
    If weaponBase != None
        OSFDefeatAcceptanceProbe.Record("shadows-miss-equipped", weaponBase == knife, player, weaponBase.GetFormID())
    EndIf
    OSFDefeatAcceptanceProbe.Record("shadows-miss-controls", Game.IsFightingControlsEnabled(), player)
    OSFDefeatAcceptanceProbe.Record("shadows-miss-truce", OSFDefeat.IsRecoveryTruceActive(player), player)
    OSFDefeatAcceptanceProbe.Record("shadows-miss-ready", armed && Game.IsFightingControlsEnabled() && OSFDefeat.IsRecoveryTruceActive(player), player)
    OSFDefeatAcceptanceProbe.Record("shadows-attack-registration", registered, player)
    OSFDefeatAcceptanceProbe.Record("shadows-weapon-drawn", player.IsWeaponDrawn(), player)
EndFunction

Function ShadowsNegativeEligibility()
    Bool ok = !hunt.Eligible(None) && !hunt.Eligible(Game.GetPlayer())
    Actor candidate = NewShadow()
    If candidate == None
        OSFDefeatAcceptanceProbe.Record("shadows-negative-eligibility", False)
        Return
    EndIf
    candidate.SetProtected(True)
    Bool protectedRef = OSFDefeat.HasProtectionOverride(candidate) && !hunt.Eligible(candidate)
    candidate.SetProtected(False)
    Bool clearedOverride = OSFDefeat.HasProtectionOverride(candidate) && !hunt.Eligible(candidate)
    OSFDefeatAcceptanceProbe.Record("shadows-protected-reference", protectedRef, candidate)
    OSFDefeatAcceptanceProbe.Record("shadows-cleared-protection-override", clearedOverride, candidate)
    OSFDefeat.ResumeActorAI(candidate)
    Utility.Wait(0.1)
    candidate.Disable()
    candidate.Delete()
    candidate = NewShadow()
    If candidate == None
        OSFDefeatAcceptanceProbe.Record("shadows-negative-eligibility", False)
        Return
    EndIf
    candidate.SetGhost(True)
    Bool ghost = !hunt.Eligible(candidate)
    candidate.SetGhost(False)
    candidate.SetPlayerTeammate(True)
    Bool teammate = !hunt.Eligible(candidate)
    candidate.SetPlayerTeammate(False)
    ; Force into an unrelated alias before eligibility, without promoting it.
    hunt.StolenWeapon.ForceRefTo(candidate)
    Bool aliased = OSFDefeat.HasQuestAlias(candidate) && !hunt.Eligible(candidate)
    hunt.StolenWeapon.Clear()
    OSFDefeat.ResumeActorAI(candidate)
    Utility.Wait(0.1)
    candidate.Disable()
    candidate.Delete()
    OSFDefeatAcceptanceProbe.Record("shadows-negative-eligibility", ok && protectedRef && clearedOverride && ghost && teammate && aliased)
EndFunction

Event OnAnimationEvent(ObjectReference source, String eventName)
    OSFDefeatAcceptanceProbe.Record("shadows-animation-" + eventName, source == Game.GetPlayer(), Game.GetPlayer())
EndEvent

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

Function ProbeDeathStates()
    hunt = Game.GetFormFromFile(0x801, "OSFDefeatShadows.esm") as OSFDefeatShadowsHunt
    If hunt == None
        OSFDefeatAcceptanceProbe.Record("death-probe-complete", False)
        Return
    EndIf
    hunt.Start()
    Utility.Wait(0.5)
    Actor subject = NewShadow()
    If subject == None
        OSFDefeatAcceptanceProbe.Record("death-probe-complete", False)
        Return
    EndIf
    Bool ok = OSFDefeatAcceptanceProbe.VerifyDeathState(subject, subject.IsDead(), False, "npc-alive")
    OSFDefeat.DefeatActor(subject, Game.GetPlayer())
    Int waited = 0
    While !OSFDefeat.IsDefeated(subject) && waited < 50
        Utility.Wait(0.1)
        waited += 1
    EndWhile
    Bool downOK = OSFDefeatAcceptanceProbe.VerifyDeathState(subject, subject.IsDead(), False, "npc-down")
    OSFDefeat.RecoverActor(subject)
    waited = 0
    While OSFDefeat.IsDefeated(subject) && waited < 50
        Utility.Wait(0.1)
        waited += 1
    EndWhile
    Bool upOK = OSFDefeatAcceptanceProbe.VerifyDeathState(subject, subject.IsDead(), False, "npc-recovered")
    subject.Kill(Game.GetPlayer())
    waited = 0
    While !subject.IsDead() && waited < 50
        Utility.Wait(0.1)
        waited += 1
    EndWhile
    Utility.Wait(0.5)
    Bool deadOK = OSFDefeatAcceptanceProbe.VerifyDeathState(subject, subject.IsDead(), True, "npc-dead")
    subject.Disable()
    subject.Delete()
    OSFDefeatAcceptanceProbe.Record("death-probe-complete", ok && downOK && upOK && deadOK)
EndFunction

Function InspectRefusalReload(String kind)
    Actor player = Game.GetPlayer()
    OSFDefeatAcceptanceProbe.VerifyDeathState(player, player.IsDead(), False, "player-" + kind + "-reloaded")
    InspectRecoveredControls()
EndFunction

Function PrepareDomain(String kind)
    Actor player = Game.GetPlayer()
    hunt = Game.GetFormFromFile(0x801, "OSFDefeatShadows.esm") as OSFDefeatShadowsHunt
    Quest spike = Game.GetFormFromFile(0x800, "OSFDefeatPersistenceSpike.esm") as Quest
    SpaceshipReference ship = Game.GetPlayerHomeSpaceShip()
    If hunt == None || spike == None || ship == None
        OSFDefeatAcceptanceProbe.Record("domain-fixture-missing", False)
        Return
    EndIf
    ObjectReference marker = (spike.GetAlias(2) as ReferenceAlias).GetReference()
    If marker == None || marker.GetCurrentShipRef() != ship || !ship.IsLanded() || player.GetCurrentPlanet() != ship.GetCurrentPlanet()
        OSFDefeatAcceptanceProbe.Record("domain-marker-invalid", False)
        Return
    EndIf
    player.MoveTo(marker)
    Utility.Wait(2.0)
    If kind == "space"
        ship.TakeOff()
        Int waited = 0
        While (ship.IsLanded() || !player.IsInSpaceEx(False)) && waited < 600
            Utility.Wait(0.1)
            waited += 1
        EndWhile
        Utility.Wait(2.0)
    EndIf
    Bool domainOK = player.GetCurrentShipRef() == ship && player.GetParentCell().IsInterior()
    If kind == "landed"
        domainOK = domainOK && ship.IsLanded() && !player.IsInSpaceEx(False)
    Else
        domainOK = domainOK && !ship.IsLanded() && player.IsInSpaceEx(False)
    EndIf
    OSFDefeatAcceptanceProbe.Record("domain-ship-landed", ship.IsLanded())
    OSFDefeatAcceptanceProbe.Record("domain-space", player.IsInSpaceEx(False))
    OSFDefeatAcceptanceProbe.Record("domain-space-including-dungeons", player.IsInSpaceEx(True))
    OSFDefeatAcceptanceProbe.Record("domain-on-foot-controls", Game.IsMovementControlsEnabled() && Game.IsFightingControlsEnabled())
    If !domainOK
        OSFDefeatAcceptanceProbe.Record("domain-fixture-invalid", False)
        Return
    EndIf
    hunt.Start()
    Utility.Wait(0.5)
    shadow = NewShadow()
    If shadow == None
        OSFDefeatAcceptanceProbe.Record("domain-aggressor-missing", False)
        Return
    EndIf
    testWeapon = Game.GetFormFromFile(0x476C4, "Starfield.esm") as Weapon
    player.RemoveItem(testWeapon, player.GetItemCount(testWeapon), True)
    player.AddItem(testWeapon, 1, True)
    player.AttachModToInventoryItem(testWeapon, Game.GetFormFromFile(0x14127D, "Starfield.esm") as ObjectMod)
    player.AttachModToInventoryItem(testWeapon, Game.GetFormFromFile(0xF2013, "Starfield.esm") as ObjectMod)
    player.AttachModToInventoryItem(testWeapon, Game.GetFormFromFile(0xFEA04, "Starfield.esm") as ObjectMod)
    player.AttachModToInventoryItem(testWeapon, Game.GetFormFromFile(0xFA8D6, "Starfield.esm") as ObjectMod)
    player.EquipItem(testWeapon, False, True)
    OSFDefeatAcceptanceProbe.VerifyWeapon(player, testWeapon, "baseline")
    repeatCredits = player.GetItemCount(Game.GetCredits())
    shadowCredits = shadow.GetItemCount(Game.GetCredits())
    defeatDay = Utility.GetCurrentGameTime()
    RegisterForRemoteEvent(player, "OnDying")
    RegisterForRemoteEvent(player, "OnDeath")
    OSFDefeatAcceptanceProbe.VerifyDeathState(player, player.IsDead(), False, "player-" + kind + "-alive")
    OSFDefeatAcceptanceProbe.Record("domain-ready", !player.IsDead() && !OSFDefeat.IsDefeated(player) && player.GetEquippedWeapon() == testWeapon, shadow)
EndFunction

Event Actor.OnDying(Actor sender, ObjectReference killer)
    OSFDefeatAcceptanceProbe.Record("domain-player-dying", sender == Game.GetPlayer(), sender)
EndEvent

Event Actor.OnDeath(Actor sender, ObjectReference killer)
    OSFDefeatAcceptanceProbe.Record("domain-player-death", sender == Game.GetPlayer(), sender)
EndEvent

Function WatchRefusal(String kind)
    Actor player = Game.GetPlayer()
    OSFDefeatShadowsAftermath aftermath = Game.GetFormFromFile(0x800, "OSFDefeatShadows.esm") as OSFDefeatShadowsAftermath
    Int waited = 0
    While (aftermath.LastResult != "unsupported-domain" || aftermath.IsRunning()) && waited < 100
        Utility.Wait(0.1)
        waited += 1
    EndWhile
    waited = 0
    While !player.IsDead() && waited < 30
        Utility.Wait(0.1)
        waited += 1
    EndWhile
    OSFDefeatAcceptanceProbe.Record("domain-player-dead", player.IsDead(), player)
    OSFDefeatAcceptanceProbe.VerifyDeathState(player, player.IsDead(), True, "player-" + kind + "-dead")
    Bool stopped = aftermath.LastResult == "unsupported-domain" && !aftermath.IsRunning() && aftermath.Phase == 0
    Bool creditsOK = player.GetItemCount(Game.GetCredits()) == repeatCredits && shadow.GetItemCount(Game.GetCredits()) == shadowCredits
    Bool huntOK = hunt.Nemesis.GetReference() == None && hunt.StolenWeapon.GetReference() == None && !hunt.IsObjectiveDisplayed(10)
    huntOK = huntOK && !player.HasSpell(hunt.PlayerDebuff) && !shadow.HasSpell(hunt.NemesisBuff)
    Bool weaponOK = player.GetItemCount(testWeapon) == 1 && OSFDefeatAcceptanceProbe.VerifyWeapon(player, testWeapon, "refused-" + kind)
    Float hours = (Utility.GetCurrentGameTime() - defeatDay) * 24.0
    Bool timeOK = hours >= 0.0 && hours < 0.1
    OSFDefeatAcceptanceProbe.Record("domain-refusal-stopped", stopped)
    OSFDefeatAcceptanceProbe.Record("domain-refusal-credits", creditsOK)
    OSFDefeatAcceptanceProbe.Record("domain-refusal-empty-hunt", huntOK)
    OSFDefeatAcceptanceProbe.Record("domain-refusal-weapon", weaponOK)
    OSFDefeatAcceptanceProbe.Record("domain-refusal-no-time-skip", timeOK, None, hours)
    OSFDefeatAcceptanceProbe.Record("domain-refused-" + kind, stopped && creditsOK && huntOK && weaponOK && timeOK)
EndFunction
