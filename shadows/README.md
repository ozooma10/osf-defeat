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
comparison; theft preservation for other weapon families still needs coverage.
The focused `-LootOnly` suite passes 22 assertions covering exact credit bounds,
unarmed/missing/duplicate weapons, equal/better enemy weapons, ammunition
preservation and equipped mining suit/helmet/boost-pack retention. It uses the
existing quiet-cell seed; quest-item refusal still needs a dedicated fixture.

Essential, protected, unique, quest-aliased, commanded, child, teammate, ghost,
scene-bound, and more-than-15-levels-higher actors are excluded. Native reservation
checks conservatively reject retained alias metadata and any per-reference
protection override, including cleared overrides.

The aftermath fades out, passes at least two and less than three hours (the engine
rounds to hour boundaries), recovers in place and grants a
30-second truce. Native damage/discharge observers and player fire, animation and
hostile-spell callbacks end the truce on attack. The broader weapon and scripted
matrices cover the tested attack families, explicit remote-cast blame and controls.
Truce save/load and expiry pass in the disposable-cell suite. Anonymous damage and
custom producers outside those paths must call `EndRecoveryTruce` before attacking.
Ship interiors and space refuse this loop and recover the framework state before
requesting normal death. Landed and orbital on-foot refusal and automatic reload
pass in the disposable Frontier fixture; piloting and hostile boarding remain open.

External quest interruption and an early-blackout save/reload restore recovery
and controls without another theft/time skip. This requires OSF Animation
`fcf9714` or later, which repairs its owned input layer after load. The
[acceptance record](../docs/MINIMAL-SHADOWS-IMPLEMENTATION.md) lists evidence and
remaining gates; the [world probe](../tests/world-shadows/README.md) is ready for
existing-enemy saves at an authored combat site and a generated enemy structure.

The plugin has no startup quest or world edits. Its priority-30 consequence starts
the repeatable aftermath quest, which starts the persistent hunt manager as needed.
The hunt retains a saved cleanup reference even if its alias disappears. External
shutdown removes owned abilities, unregisters events, stops its timer and resets
the stopped journal; starting it again restores attack subscriptions. Alias loss
lifts the wound without granting a kill reward. Explicit `CancelHunt` also removes
the earned reward and truce; requests during a mutation remain queued.

Once the blackout has ended, call the hunt quest's `CancelHunt` and the framework's
`ForceRecoverAll` before disabling a tested installation. Verify that owned effects,
aliases and truce are gone, then save. Stolen loot stays where it was transferred.
