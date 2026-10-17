Scriptname OSFDefeatScriptedAcceptance extends ObjectReference

OSFDefeatShadowsHunt hunt
Actor target
ActorValue health
Weapon weaponBase
Spell damageSpell
Spell healingSpell
Float playerBefore
Float targetBefore

Function Initialize()
    Actor player = Game.GetPlayer()
    health = Game.GetFormFromFile(0x2D4, "Starfield.esm") as ActorValue
    weaponBase = Game.GetFormFromFile(0x476C4, "Starfield.esm") as Weapon
    ; Installed .244 forms: DamageHealthSpell (40), Dueling_RestoreHealth_Spell.
    damageSpell = Game.GetFormFromFile(0x29AAE9, "Starfield.esm") as Spell
    healingSpell = Game.GetFormFromFile(0x1E1171, "Starfield.esm") as Spell
    hunt = Game.GetFormFromFile(0x801, "OSFDefeatShadows.esm") as OSFDefeatShadowsHunt
    target = player.PlaceAtMe(Game.GetFormFromFile(0x270255, "Starfield.esm"), 1, False, True, False) as Actor
    If target == None || hunt == None || weaponBase == None || damageSpell == None || healingSpell == None
        OSFDefeatAcceptanceProbe.Record("scripted-fixture", False)
        Return
    EndIf
    target.Enable()
    target.MoveTo(player, 5.0, 0.0, 0.0)
    target.SetRestrained(True)
    OSFDefeat.SuspendActorAI(target)
    hunt.Start()
    hunt.WatchPlayer()
    player.AddItem(weaponBase, 1, True)
    player.AddItem(weaponBase.GetAmmo(), 500, True)
    player.EquipItem(weaponBase, False, True)
    player.DrawWeapon()
    target.AddItem(weaponBase, 1, True)
    target.AddItem(weaponBase.GetAmmo(), 500, True)
    target.EquipItem(weaponBase, False, True)
    ; Active effects need the NPC's process running. Restrain the disposable
    ; target to prevent ordinary attacks while leaving effect updates enabled.
    OSFDefeat.ResumeActorAI(target)
    RegisterForRemoteEvent(player, "OnSpellCast")
    Utility.Wait(2.0)
    OSFDefeatAcceptanceProbe.Record("scripted-damage-hostile", damageSpell.IsHostile())
    OSFDefeatAcceptanceProbe.Record("scripted-healing-hostile", healingSpell.IsHostile())
    OSFDefeatAcceptanceProbe.Record("scripted-fixture", target.Is3DLoaded() && !target.IsDead() && !OSFDefeat.IsActorAISuspended(target), target)
EndFunction

Event ObjectReference.OnSpellCast(ObjectReference sender, Form spellCast)
    OSFDefeatAcceptanceProbe.Record("scripted-spell-event", sender == Game.GetPlayer(), Game.GetPlayer(), spellCast.GetFormID())
EndEvent

Function Prepare(String kind)
    Actor player = Game.GetPlayer()
    player.SetValue(health, 5000.0)
    player.RestoreValue(health, 5000.0)
    player.DamageValue(health, 100.0)
    target.SetValue(health, 5000.0)
    target.RestoreValue(health, 5000.0)
    target.DamageValue(health, 100.0)
    hunt.LastAttackEvent = ""
    Utility.Wait(0.5)
    playerBefore = player.GetValue(health)
    targetBefore = target.GetValue(health)
    OSFDefeatAcceptanceProbe.Record("scripted-ready-" + kind, OSFDefeat.IsRecoveryTruceActive(player) && !player.IsDead() && !target.IsDead() && hunt.LastAttackEvent == "", target)
EndFunction

Function Run(String kind)
    Actor player = Game.GetPlayer()
    If kind == "native-attributed"
        OSFDefeatAcceptanceProbe.ApplyScriptedDamage(target, player, 0)
    ElseIf kind == "native-restore"
        OSFDefeatAcceptanceProbe.ApplyScriptedDamage(target, player, 1)
    ElseIf kind == "native-nonhealth"
        OSFDefeatAcceptanceProbe.ApplyScriptedDamage(target, player, 2)
    ElseIf kind == "papyrus-anonymous"
        target.DamageValue(health, 5.0)
    ElseIf kind == "papyrus-cooperative"
        OSFDefeat.EndRecoveryTruce(player)
        target.DamageValue(health, 5.0)
    ElseIf kind == "incoming-spell"
        damageSpell.Cast(target, player)
    ElseIf kind == "spell-cast"
        damageSpell.Cast(player, target)
    ElseIf kind == "spell-remote"
        damageSpell.RemoteCast(target, player, target)
    ElseIf kind == "combat-spell"
        player.DoCombatSpellApply(damageSpell, target)
    ElseIf kind == "spell-heal"
        healingSpell.Cast(player, player)
    ElseIf kind == "combat-spell-heal"
        player.DoCombatSpellApply(healingSpell, player)
    ElseIf kind == "weapon-fire-player"
        weaponBase.Fire(player, weaponBase.GetAmmo())
    ElseIf kind == "weapon-fire-npc"
        weaponBase.Fire(target, weaponBase.GetAmmo())
    EndIf
    Utility.Wait(1.0)
    Float playerDelta = player.GetValue(health) - playerBefore
    Float targetDelta = target.GetValue(health) - targetBefore
    Bool effect = targetDelta < -0.1
    If kind == "native-restore"
        effect = targetDelta > 0.1
    ElseIf kind == "native-nonhealth"
        effect = targetDelta == 0.0 && playerDelta == 0.0
    ElseIf kind == "incoming-spell"
        effect = playerDelta < -0.1
    ElseIf kind == "spell-heal" || kind == "combat-spell-heal"
        effect = playerDelta > 0.1
    ElseIf kind == "weapon-fire-player" || kind == "weapon-fire-npc"
        ; The native WeaponFired event is additionally required by the driver.
        effect = True
    EndIf
    OSFDefeatAcceptanceProbe.Record("scripted-player-delta-" + kind, True, player, playerDelta)
    OSFDefeatAcceptanceProbe.Record("scripted-target-delta-" + kind, True, target, targetDelta)
    OSFDefeatAcceptanceProbe.Record("scripted-truce-after-" + kind, OSFDefeat.IsRecoveryTruceActive(player), player)
    OSFDefeatAcceptanceProbe.Record("scripted-callback-" + kind + "-" + hunt.LastAttackEvent, True)
    OSFDefeatAcceptanceProbe.Record("scripted-observed-" + kind, effect && !player.IsDead() && !target.IsDead(), target)
EndFunction

Function InspectCleanup()
    Utility.Wait(2.0)
    Actor player = Game.GetPlayer()
    OSFDefeatAcceptanceProbe.Record("scripted-cleanup", !OSFDefeat.IsDefeated(player) && !OSFDefeat.IsRecoveryTruceActive(player) && Game.IsMovementControlsEnabled() && Game.IsFightingControlsEnabled() && Game.IsLookingControlsEnabled())
EndFunction
