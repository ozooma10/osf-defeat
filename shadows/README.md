# Minimal Shadows — staged slice 1

This pack is under development. It is built into a separate payload and has not
replaced Basic Consequences in the shipped installation.

Run `pwsh -File tools/build-shadows.ps1` from the repository root. The build
compiles Papyrus, creates `OSFDefeatShadows.esm`, round-trips it through Spriggit
0.40.1, checks its record links, and emits a payload and receipt in `build/shadows/`.
It does not deploy or modify an MO2 profile.

The first loop uses one actual aggressor held in a persistent alias with the title
**The Unbroken**. Its **Unbroken** ability adds 40 physical resistance. The player
receives **Shaken** (-20 physical resistance) until the nemesis dies. A player kill
earns **Hardened Resolve** (+10 physical resistance) if the target is at least the
player's level at death; another killer removes the wound without a reward.
These are pack-owned abilities, not vanilla afflictions. One existing hunt cannot
be replaced by another defeat.

An eligible promotion transfers 10% of credits (25 minimum, 2,500 maximum, bounded
by available credits). Weapon theft compares base credit values in this first
version. The framework snapshots the equipped base before down presentation changes
it. Theft requires exactly one owned copy, holds it in an alias, and moves that
inventory item by base form; ambiguous stacks and quest items are skipped. Suit,
helmet, and boost pack are never taken. The modified/legendary Eon fixture preserves
every OMOD row through theft, reload, retrieval and forced world pickup.
Base value is not a damage or modification-quality
comparison; other weapon families still need coverage.

Essential, protected, unique, quest-aliased, commanded, child, teammate, ghost,
scene-bound, and more-than-15-levels-higher actors are excluded. Native reservation
checks conservatively reject retained alias metadata and any per-reference
protection override, including cleared overrides.

The aftermath fades out, passes at least two and less than three hours (the engine
rounds to hour boundaries), recovers in place and grants a
30-second truce. The native damage path and player fire/animation callbacks end
the truce on attack. Missed knife, unarmed, Eon and frag attacks, truce save/load
and expiry pass in the disposable-cell suite. Frag release emits `WeaponFire`. Scripted producers that
bypass these callbacks must call `EndRecoveryTruce` before attacking.
Ship interiors and space refuse this loop and recover the framework state before
requesting normal death. That refusal path also needs a dedicated live fixture.

External quest interruption and an early-blackout save/reload restore recovery
and controls without another theft/time skip. This requires OSF Animation
`fcf9714` or later, which repairs its owned input layer after load. The
[acceptance record](../docs/MINIMAL-SHADOWS-IMPLEMENTATION.md) lists evidence and
remaining gates; the [world probe](../tests/world-shadows/README.md) is ready for
existing-enemy saves at an authored combat site and a generated enemy structure.

The plugin has no startup quest or world edits. Its priority-30 consequence starts
the repeatable aftermath quest, which starts the persistent hunt manager as needed.
Before disabling a tested installation, call the hunt quest's `CancelHunt` and
the framework's `ForceRecoverAll`, then save. Stolen loot stays in the NPC inventory.
