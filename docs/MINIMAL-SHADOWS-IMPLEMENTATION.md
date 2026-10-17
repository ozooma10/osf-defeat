# Minimal Shadows implementation

Slice 1 of [CORE-EXPERIENCE.md](CORE-EXPERIENCE.md), implemented as a separate
[staged pack](../shadows/README.md). Basic Consequences remains the shipped pack.
The first complete disposable-cell loop passed on 2026-10-16 (UTC receipts dated
2026-10-17). Broader world acceptance remains open below.

## Implemented loop

- One persistent actual-aggressor alias, titled **The Unbroken**, with a hunt objective.
  A second promotion cannot replace an occupied slot or remove its debuff.
- **Unbroken** gives the nemesis +40 physical resistance; **Shaken** gives the player
  -20. A player kill earns **Hardened Resolve** (+10) only when the target is at least
  the player's level at death. Another killer clears the debuff without that reward.
  These are pack-owned abilities, not curable vanilla afflictions.
- Conservative admission excludes essential/protected/unique actors, quest aliases,
  protection overrides, teammates, commanded actors, children, ghosts, scenes and
  targets more than 15 levels above the player.
- Promotion takes 10% of credits, clamped to 25–2,500 and available funds. Weapon
  theft uses the pre-down equipped-base snapshot, requires exactly one owned copy,
  skips quest items, compares base credit value, and moves the inventory item to the
  nemesis. This quality proxy does not evaluate damage or modifications. Suit,
  helmet and boost pack are never taken.
- A repeatable aftermath fades out, advances at least two and less than three game
  hours, then recovers in place with a 30-second truce. PassTime rounds to an hour
  boundary, so the script corrects a short advance with one additional hour. A
  12,000-second game-clock lease accommodates that skip; a separate 30-second
  Papyrus timer covers stalled aftermath work and restores the fade.
- Ship interiors and space refuse this loop, restore framework state, then request
  normal death. This is a staged content-level refusal, not a native domain gate.

`tools/build-shadows.ps1` compiles the scripts, builds and round-trips the nine-record
ESM with pinned Spriggit, then independently checks record links, aliases, objectives,
spell magnitudes and VMAD bindings. Output stays under `build/shadows/`; this helper
does not deploy. The runtime runner installs only into its owned disposable profile.

## Recovery truce foundation

`RecoverActorWithTruce(actor, seconds)` atomically recovers a defeated actor while
retaining captive-faction pacification. A finite positive duration up to 3,600 seconds is converted
at the current time scale into a saved game-day deadline. It does not grant
damage immunity. Query `IsRecoveryTruceActive` as well as `IsDefeated` to confirm
the queued request completed. Missing pacification leaves recovery held so the
existing watchdog remains responsible for releasing the actor.

Outgoing health damage queues truce cancellation after the original damage write,
with the existing save-epoch guard. Content must call `EndRecoveryTruce` on fire
and attack events as well, to cover misses. New defeat and `ForceRecoverAll` end
the truce. Pre-existing faction memberships are preserved; unresolved actors
retain cleanup tracking until they resolve. Save record `TRC0` v1 carries actor
ID, game-day deadline, and faction ownership; `CPT0` retains the exact faction ID.

`HasQuestAlias` and `HasProtectionOverride` are conservative metadata-presence tests:
retained alias metadata and cleared reference overrides can still exclude an actor.
`GetLastPlayerEquippedWeapon` captures the owned primary base before down presentation;
it is transient handoff context, not an item-instance identifier. See the
[Papyrus API](papyrus-api.md).

## Validation

- Offline: **83 native cases / 1,014 checks**, **97 runner checks**, payload validation,
  production DLL build and Papyrus compilation passed. Receipt:
  `build/smoke/20261017-013301-e995c8452c3c4586891cfa3c8957b513/summary.json`.
- Focused live suite: **15/15 passed** on a fresh locally owned MO2/SFSE session.
  Receipt: `build/shadows-runtime-13.json`; artifacts:
  `../OSF Test Harness/artifacts/20261017-013541-719-DefeatSmoke`.
- The live run verified promotion, objective, both resistance deltas, credits, unique
  Eon transfer/equipping, blackout time, controls, pacification with active hostile AI,
  slot preservation, real truce save/load, missed knife-swing cancellation, timer
  expiry, player reward, other-killer cleanup, weapon retrieval and healthy-load cleanup.
  Weapon retrieval in this passing run used corpse inventory. The test also permits
  pickup of the identical aliased reference if the engine drops it; that branch is
  not yet independently certified.

Live iteration caught the down-state replacing the weapon getter with unarmed,
PassTime advancing less than requested, and suspended test AI preventing ability/death
processing. Inventory proxies can report no container even after a successful move;
the test uses actual inventory counts and equipped state. Canonical engine evidence
is in OSF RE's `gameplay.actor_equipment` module. Failed receipts remain retained.

## Acceptance before replacing Basic

- Existing world aggressor at an authored encounter and a generated enemy structure:
  normal appearance, travel/return, reload, longer absence and actual reset behavior.
  Slice 0 proved an inert surrogate at an authored city and an outdoor natural feature.
- Modified/legendary weapon preservation and the world-drop retrieval branch.
- Missed unarmed, ranged and grenade attacks; third-party attack producers.
- Negative eligibility fixtures, especially reference protection overrides; lower-level
  player-kill refusal and a second full defeat while the hunt is occupied.
- Ship/space refusal, saves during blackout, external quest interruption, and visible
  title/objective/ability presentation in ordinary play.

Next slice remains the wake-up table once this staged loop clears those gates. The
five-slot library, dropped pack, companion rescue, settings and wider traits belong
to later slices. Before disabling this test pack, call the hunt's `CancelHunt`, then
`ForceRecoverAll`, verify cleanup and save. Stolen loot stays where it was transferred.
