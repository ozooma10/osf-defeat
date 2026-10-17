Scriptname OSFDefeatLifecycleAcceptance extends ObjectReference

OSFDefeatShadowsHunt hunt
Actor target
Actor previous
Weapon weaponBase
Int playerCredits
Int targetCredits

Function Initialize()
    hunt = Game.GetFormFromFile(0x801, "OSFDefeatShadows.esm") as OSFDefeatShadowsHunt
    weaponBase = Game.GetFormFromFile(0x476C4, "Starfield.esm") as Weapon
    OSFDefeatAcceptanceProbe.Record("lifecycle-fixture", hunt != None && weaponBase != None)
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
            If candidate.GetLevel() >= Game.GetPlayer().GetLevel() && hunt.Eligible(candidate)
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

Bool Function FreshHunt()
    ; Isolation repairs happen only AFTER the preceding observation, so an
    ; expected failure does not prevent the remaining independent checks.
    hunt.CancelHunt()
    If target != None
        target.RemoveSpell(hunt.NemesisBuff)
        OSFDefeat.ResumeActorAI(target)
    EndIf
    previous = target
    If !hunt.IsRunning()
        hunt.Start()
    EndIf
    Utility.Wait(0.5)
    target = NewTarget()
    If target == None
        Return False
    EndIf
    Actor player = Game.GetPlayer()
    Weapon old = target.GetEquippedWeapon()
    If old != None
        target.UnequipItem(old, True, True)
        target.RemoveItem(old, target.GetItemCount(old), True)
    EndIf
    target.RemoveItem(weaponBase, target.GetItemCount(weaponBase), True)
    player.RemoveItem(weaponBase, player.GetItemCount(weaponBase), True)
    player.AddItem(weaponBase, 1, True)
    player.RemoveItem(Game.GetCredits(), player.GetItemCount(Game.GetCredits()), True)
    player.AddItem(Game.GetCredits(), 1000, True)
    Int priorCredits = target.GetItemCount(Game.GetCredits())
    Bool promoted = hunt.Promote(target, weaponBase)
    Utility.Wait(0.5)
    playerCredits = player.GetItemCount(Game.GetCredits())
    targetCredits = target.GetItemCount(Game.GetCredits())
    Return promoted && Active() && playerCredits == 900 && targetCredits == priorCredits + 100 && hunt.LastTheftStatus == 7
EndFunction

Bool Function Active()
    Return hunt.IsRunning() && hunt.Nemesis.GetActorReference() == target && hunt.StolenWeapon.GetReference() != None && target.HasSpell(hunt.NemesisBuff) && Game.GetPlayer().HasSpell(hunt.PlayerDebuff) && hunt.IsObjectiveDisplayed(10) && !hunt.IsObjectiveCompleted(10)
EndFunction

Bool Function Clean()
    Return hunt.Nemesis.GetReference() == None && hunt.StolenWeapon.GetReference() == None && !target.HasSpell(hunt.NemesisBuff) && !Game.GetPlayer().HasSpell(hunt.PlayerDebuff) && !Game.GetPlayer().HasSpell(hunt.PlayerReward) && !hunt.IsObjectiveDisplayed(10) && !OSFDefeat.IsRecoveryTruceActive(Game.GetPlayer())
EndFunction

Bool Function LootUnchanged()
    Return Game.GetPlayer().GetItemCount(Game.GetCredits()) == playerCredits && target.GetItemCount(Game.GetCredits()) == targetCredits && Game.GetPlayer().GetItemCount(weaponBase) == 0 && target.GetItemCount(weaponBase) == 1
EndFunction

Function ObserveCleanup(String label)
    OSFDefeatAcceptanceProbe.Record(label + "-alias", hunt.Nemesis.GetReference() == None)
    OSFDefeatAcceptanceProbe.Record(label + "-weapon-alias", hunt.StolenWeapon.GetReference() == None)
    OSFDefeatAcceptanceProbe.Record(label + "-buff", !target.HasSpell(hunt.NemesisBuff), target)
    OSFDefeatAcceptanceProbe.Record(label + "-debuff", !Game.GetPlayer().HasSpell(hunt.PlayerDebuff))
    OSFDefeatAcceptanceProbe.Record(label + "-reward", !Game.GetPlayer().HasSpell(hunt.PlayerReward))
    OSFDefeatAcceptanceProbe.Record(label + "-objective", !hunt.IsObjectiveDisplayed(10))
    OSFDefeatAcceptanceProbe.Record(label + "-objective-failed", hunt.IsObjectiveFailed(10))
    OSFDefeatAcceptanceProbe.Record(label + "-objective-completed", hunt.IsObjectiveCompleted(10))
    OSFDefeatAcceptanceProbe.Record(label + "-truce", !OSFDefeat.IsRecoveryTruceActive(Game.GetPlayer()))
    OSFDefeatAcceptanceProbe.Record(label + "-loot", LootUnchanged(), target)
EndFunction

Function Prepare(String kind)
    Bool ready = True
    If kind != "cancel-idempotent" && kind != "stopped-promotion" && kind != "reward-cancel" && kind != "restart-attack"
        ready = FreshHunt()
    EndIf
    OSFDefeatAcceptanceProbe.Record("lifecycle-ready-" + kind, ready, target)
EndFunction

Function Run(String kind)
    Bool ok = True
    If kind == "cancel-idempotent"
        hunt.CancelHunt()
        hunt.CancelHunt()
        ok = Clean() && LootUnchanged()
    ElseIf kind == "reward-cancel"
        ok = Game.GetPlayer().HasSpell(hunt.PlayerReward) && hunt.IsObjectiveCompleted(10)
        hunt.CancelHunt()
        ok = ok && Clean() && !hunt.IsObjectiveCompleted(10) && !hunt.IsObjectiveFailed(10)
    ElseIf kind == "stopped-promotion"
        ; Keep the quest stopped, repair only the previous test's leftovers.
        hunt.CancelHunt()
        target.RemoveSpell(hunt.NemesisBuff)
        Actor candidate = NewTarget()
        ok = candidate != None && !hunt.IsRunning()
        If ok
            ok = !hunt.Promote(candidate) && !candidate.HasSpell(hunt.NemesisBuff) && !Game.GetPlayer().HasSpell(hunt.PlayerDebuff) && hunt.Nemesis.GetReference() == None
        EndIf
        hunt.CancelHunt()
        If candidate != None
            candidate.RemoveSpell(hunt.NemesisBuff)
            OSFDefeat.ResumeActorAI(candidate)
        EndIf
    Else
        ok = Active()
        If ok
            If kind == "cancel"
                Game.GetPlayer().AddSpell(hunt.PlayerReward, False)
                ok = OSFDefeat.IsRecoveryTruceActive(Game.GetPlayer())
                hunt.CancelHunt()
                ok = ok && Clean() && LootUnchanged()
            ElseIf kind == "alias-loss"
                hunt.Nemesis.Clear()
                Utility.Wait(6.0)
                ok = Clean() && LootUnchanged() && !hunt.IsObjectiveCompleted(10)
            ElseIf kind == "stop"
                hunt.Stop()
                ; Stop queues OnQuestShutdown; wait for its bounded completion.
                Int waited = 0
                While (!Clean() || hunt.IsRunning()) && waited < 50
                    Utility.Wait(0.2)
                    waited += 1
                EndWhile
                ObserveCleanup("lifecycle-stop-observed")
                ok = !hunt.IsRunning() && Clean() && LootUnchanged()
            ElseIf kind == "restart-old-death"
                ok = previous != None && previous != target
                If ok
                    previous.Kill(Game.GetPlayer())
                    OSFDefeat.ResumeActorAI(previous)
                    Utility.Wait(2.0)
                    ok = previous.IsDead() && Active() && LootUnchanged() && !Game.GetPlayer().HasSpell(hunt.PlayerReward)
                EndIf
            ElseIf kind == "restart-attack"
                hunt.LastAttackEvent = ""
                Spell attack = Game.GetFormFromFile(0x29AAE9, "Starfield.esm") as Spell
                ok = attack != None && OSFDefeat.IsRecoveryTruceActive(Game.GetPlayer())
                Game.GetPlayer().DoCombatSpellApply(attack, target)
                Utility.Wait(1.0)
                ; Require the actual remote callback after shutdown/start. Native
                ; damage cancellation alone cannot prove subscriptions returned.
                ok = ok && hunt.LastAttackEvent == "OnSpellCast" && !OSFDefeat.IsRecoveryTruceActive(Game.GetPlayer()) && Active()
            ElseIf kind == "reward-idle"
                target.Kill(Game.GetPlayer())
                OSFDefeat.ResumeActorAI(target)
                Utility.Wait(7.0)
                ok = target.IsDead() && hunt.Nemesis.GetReference() == None && !target.HasSpell(hunt.NemesisBuff) && !Game.GetPlayer().HasSpell(hunt.PlayerDebuff) && Game.GetPlayer().HasSpell(hunt.PlayerReward) && hunt.IsObjectiveCompleted(10)
            EndIf
        EndIf
    EndIf
    OSFDefeatAcceptanceProbe.Record("lifecycle-" + kind, ok, target)
EndFunction

Function PrepareSave()
    Bool ready = FreshHunt()
    OSFDefeat.ResumeActorAI(target)
    Utility.Wait(0.5)
    OSFDefeatAcceptanceProbe.Record("lifecycle-active-before-save", ready && Active() && !OSFDefeat.IsActorAISuspended(target), target)
EndFunction

Function InspectLoadedActive()
    Utility.Wait(6.0)
    OSFDefeatAcceptanceProbe.Record("lifecycle-active-load", Active() && LootUnchanged(), target)
    ; Prove saved ownership can clean the actor even after the alias disappears.
    hunt.Nemesis.Clear()
    hunt.CancelHunt()
    ObserveCleanup("lifecycle-loaded-observed")
    OSFDefeatAcceptanceProbe.Record("lifecycle-loaded-cancel", Clean() && LootUnchanged(), target)
EndFunction

Function InspectLoadedClean()
    Utility.Wait(6.0)
    OSFDefeatAcceptanceProbe.Record("lifecycle-cancelled-load", Clean() && LootUnchanged(), target)
    OSFDefeatAcceptanceProbe.Record("lifecycle-restart-after-load", FreshHunt(), target)
EndFunction

Function InspectCleanup()
    Utility.Wait(2.0)
    Actor player = Game.GetPlayer()
    hunt = Game.GetFormFromFile(0x801, "OSFDefeatShadows.esm") as OSFDefeatShadowsHunt
    OSFDefeatAcceptanceProbe.Record("lifecycle-cleanup", !player.HasSpell(hunt.PlayerDebuff) && !player.HasSpell(hunt.PlayerReward) && hunt.Nemesis.GetReference() == None && hunt.StolenWeapon.GetReference() == None && Game.IsMovementControlsEnabled() && Game.IsFightingControlsEnabled() && Game.IsLookingControlsEnabled())
EndFunction
