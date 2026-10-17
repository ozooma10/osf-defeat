Scriptname OSFDefeatWeaponAcceptance extends ObjectReference

OSFDefeatShadowsHunt hunt
Weapon attackWeapon
Ammo attackAmmo
Int ammoBefore
Int throwableBefore
Bool thrown
Bool melee

; IDs and editor IDs were read from the installed .244 Starfield.esm WEAP group.
; Resolve the actual runtime form and verify its equipment/consumption in each case.
Weapon Function ResolveWeapon(String kind)
    Int id = 0
    If kind == "pistol-eon" || kind == "bash-eon"
        id = 0x476C4 ; Eon
    ElseIf kind == "rifle-grendel"
        id = 0x28A02 ; Grendel
    ElseIf kind == "shotgun-coachman"
        id = 0x26D96B ; Coachman
    ElseIf kind == "shotgun-breach"
        id = 0x278F74 ; pumpshotgun
    ElseIf kind == "laser-equinox"
        id = 0x1BC4F ; EquiNOX
    ElseIf kind == "particle-inflictor"
        id = 0x26D8A0 ; InflictorRifle
    ElseIf kind == "magpulse"
        id = 0x23606 ; MagPulse
    ElseIf kind == "magstorm"
        id = 0x26035E ; MagStorm
    ElseIf kind == "heavy-microgun"
        id = 0x26D96C ; M1919
    ElseIf kind == "heavy-bridger"
        id = 0x26D96A ; Bridger
    ElseIf kind == "em-novablast"
        id = 0x26D968 ; Novablast
    ElseIf kind == "beam-cutter"
        id = 0x16758 ; Cutter
    ElseIf kind == "melee-knife"
        id = 0x35A48 ; CombatKnife
    ElseIf kind == "melee-axe"
        id = 0x4F760 ; RescueAxe
    ElseIf kind == "melee-wakizashi"
        id = 0x26D8A1 ; Wakizashi
    ElseIf kind == "grenade-frag"
        id = 0x115EF ; FragGrenade
    ElseIf kind == "grenade-impact"
        id = 0x26D89F ; ImpactGrenade
    ElseIf kind == "grenade-shrapnel"
        id = 0x26D89D ; ShrapnelGrenade
    ElseIf kind == "grenade-particle"
        id = 0x26D89E ; ParticleGrenade
    ElseIf kind == "grenade-incendiary"
        id = 0x26F180 ; IncendiaryGrenade
    ElseIf kind == "mine-frag"
        id = 0x4A41A ; FragMine
    ElseIf kind == "mine-inferno"
        id = 0x389F33 ; InfernoMine
    ElseIf kind == "mine-cryo"
        id = 0x389F34 ; CryoMine
    ElseIf kind == "mine-tesla"
        id = 0x389F37 ; TeslaMine
    EndIf
    If id == 0
        Return None
    EndIf
    Return Game.GetFormFromFile(id, "Starfield.esm") as Weapon
EndFunction

Function ClearThrowables()
    String[] kinds = new String[9]
    kinds[0] = "grenade-frag"
    kinds[1] = "grenade-impact"
    kinds[2] = "grenade-shrapnel"
    kinds[3] = "grenade-particle"
    kinds[4] = "grenade-incendiary"
    kinds[5] = "mine-frag"
    kinds[6] = "mine-inferno"
    kinds[7] = "mine-cryo"
    kinds[8] = "mine-tesla"
    Actor player = Game.GetPlayer()
    Int i = 0
    While i < kinds.Length
        Weapon item = ResolveWeapon(kinds[i])
        If item != None
            player.RemoveItem(item, player.GetItemCount(item), True)
        EndIf
        i += 1
    EndWhile
EndFunction

Function Initialize()
    hunt = Game.GetFormFromFile(0x801, "OSFDefeatShadows.esm") as OSFDefeatShadowsHunt
    If hunt != None
        If !hunt.IsRunning()
            hunt.Start()
        EndIf
        hunt.WatchPlayer()
    EndIf
    Game.ForceFirstPerson()
    Utility.Wait(1.0)
    OSFDefeatAcceptanceProbe.Record("weapons-fixture", hunt != None && hunt.IsRunning() && Game.IsFightingControlsEnabled())
EndFunction

Function PrepareAttack(String kind)
    Actor player = Game.GetPlayer()
    ActorValue health = Game.GetFormFromFile(0x2D4, "Starfield.esm") as ActorValue
    ; Explosive misses can hit a nearby wall. Restore the exact seed via a real
    ; healthy-save load at the end; never save the raised-health fixture.
    player.SetValue(health, 5000.0)
    player.RestoreValue(health, 5000.0)
    player.UnequipAll()
    ClearThrowables()
    attackWeapon = ResolveWeapon(kind)
    thrown = kind == "grenade-frag" || kind == "grenade-impact" || kind == "grenade-shrapnel" || kind == "grenade-particle" || kind == "grenade-incendiary" || kind == "mine-frag" || kind == "mine-inferno" || kind == "mine-cryo" || kind == "mine-tesla"
    melee = kind == "melee-knife" || kind == "melee-axe" || kind == "melee-wakizashi" || kind == "unarmed" || kind == "bash-eon"
    attackAmmo = None
    ammoBefore = 0
    throwableBefore = 0
    If attackWeapon != None
        player.RemoveItem(attackWeapon, player.GetItemCount(attackWeapon), True)
        If thrown
            player.AddItem(attackWeapon, 2, True)
            throwableBefore = 2
        Else
            player.AddItem(attackWeapon, 1, True)
            attackAmmo = attackWeapon.GetAmmo()
            If attackAmmo != None
                player.AddItem(attackAmmo, 500, True)
            EndIf
        EndIf
        player.EquipItem(attackWeapon, False, True)
    EndIf
    player.DrawWeapon()
    hunt.LastAttackEvent = ""
    Utility.Wait(2.0)
    If attackAmmo != None
        ammoBefore = player.GetItemCount(attackAmmo)
    EndIf
    Weapon equipped = player.GetEquippedWeapon()
    Bool armed = False
    If kind == "unarmed"
        armed = equipped == None || equipped == Game.GetFormFromFile(0x1F4, "Starfield.esm")
    ElseIf thrown
        ; Primary slot excludes throwables. Record the secondary getters as
        ; diagnostics; exact one-item consumption is the release oracle.
        armed = attackWeapon != None && player.GetItemCount(attackWeapon) == throwableBefore
        Int slot = 1
        While slot < 3
            Weapon secondary = player.GetEquippedWeapon(slot)
            If secondary != None
                OSFDefeatAcceptanceProbe.Record("weapons-secondary-" + kind, secondary == attackWeapon, player, secondary.GetFormID())
            EndIf
            slot += 1
        EndWhile
    Else
        armed = attackWeapon != None && equipped == attackWeapon && player.GetItemCount(attackWeapon) == 1
        If !melee && kind != "beam-cutter"
            armed = armed && attackAmmo != None && ammoBefore > 0
        EndIf
    EndIf
    If attackWeapon != None
        OSFDefeatAcceptanceProbe.Record("weapons-form-" + kind, True, player, attackWeapon.GetFormID())
    EndIf
    OSFDefeatAcceptanceProbe.Record("weapons-ammo-before-" + kind, True, player, ammoBefore)
    OSFDefeatAcceptanceProbe.Record("weapons-ready-" + kind, armed && player.IsWeaponDrawn() && Game.IsFightingControlsEnabled() && OSFDefeat.IsRecoveryTruceActive(player) && hunt.LastAttackEvent == "")
EndFunction

Function InspectPassive()
    Utility.Wait(2.0)
    OSFDefeatAcceptanceProbe.Record("weapons-passive-event-" + hunt.LastAttackEvent, True)
    OSFDefeatAcceptanceProbe.Record("weapons-passive", OSFDefeat.IsRecoveryTruceActive(Game.GetPlayer()) && hunt.LastAttackEvent == "" && Game.GetPlayer().GetItemCount(attackAmmo) == ammoBefore)
EndFunction

Function InspectAttack(String kind)
    Utility.Wait(1.0)
    Actor player = Game.GetPlayer()
    String observed = hunt.LastAttackEvent
    Bool eventMatches = observed == "WeaponFire" || observed == "OnPlayerFireWeapon"
    Bool consumed = True
    If melee
        eventMatches = observed == "HitFrame" || observed == "MeleeAttackStart" || observed == "MeleeAttackBashStart"
    ElseIf thrown
        consumed = player.GetItemCount(attackWeapon) == throwableBefore - 1
        OSFDefeatAcceptanceProbe.Record("weapons-remaining-" + kind, consumed, player, player.GetItemCount(attackWeapon))
    ElseIf kind != "beam-cutter"
        Int ammoAfter = player.GetItemCount(attackAmmo)
        consumed = ammoAfter < ammoBefore
        OSFDefeatAcceptanceProbe.Record("weapons-ammo-after-" + kind, consumed, player, ammoAfter)
    EndIf
    OSFDefeatAcceptanceProbe.Record("weapons-event-" + kind + "-" + observed, eventMatches)
    If thrown || kind == "heavy-bridger"
        Utility.Wait(6.0)
    EndIf
    OSFDefeatAcceptanceProbe.Record("weapons-observed-" + kind, eventMatches && consumed && !OSFDefeat.IsRecoveryTruceActive(player) && !OSFDefeat.IsDefeated(player))
EndFunction

Function InspectCleanup()
    Utility.Wait(3.0)
    Actor player = Game.GetPlayer()
    OSFDefeatAcceptanceProbe.Record("weapons-cleanup", !OSFDefeat.IsDefeated(player) && !OSFDefeat.IsRecoveryTruceActive(player) && Game.IsMovementControlsEnabled() && Game.IsFightingControlsEnabled() && Game.IsLookingControlsEnabled())
EndFunction
