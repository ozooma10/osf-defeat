# Minimal Shadows implementation

Slice 1 of [CORE-EXPERIENCE.md](CORE-EXPERIENCE.md). Work in progress; Basic
Consequences remains the shipped pack until the new loop passes acceptance.

## Recovery truce foundation

`RecoverActorWithTruce(actor, seconds)` atomically recovers a defeated actor while
retaining captive-faction pacification. The duration is 1–3600 seconds converted
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

Offline validation: 82 native cases / 1,011 checks, 97 runner evidence checks,
payload verification, production DLL build and Papyrus compilation passed.
Receipt: `build/smoke/20261017-004329-fe86fbf65dbb40d78e1e19800cffb7e6/summary.json`.
Live truce behavior is not yet verified.

## Remaining slice work

- Persistent one-slot nemesis quest, title, trait spells, hunt objective and death cleanup.
- Repeatable blackout aftermath, theft and wake-in-place truce.
- Conservative aggressor reservation checks and instance-preserving weapon transfer.
- Runtime coverage for truce save/load and the full loop, including missed attacks,
  player/other-killer reward rules, repeat defeat and slot exhaustion.
- Existing aggressor appearance/persistence at an enemy structure remains an acceptance gate.
