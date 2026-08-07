# OSF Defeat domain vocabulary

This document is the naming contract for code, JSON, Papyrus, logs, tests, and user-facing documentation. New work uses the canonical terms below. Compatibility names remain supported only at explicit boundaries and must be described as legacy aliases.

| Canonical term | Meaning | Do not use for new work |
|---|---|---|
| defeated actor | An actor currently registered in the framework's down-state | victim, entry |
| active defeat | The runtime record for one defeated actor | victim data, defeat state |
| defeat phase | The active defeat's mutation phase: applying, active, or recovering | state when referring to this enum |
| recovery | Ending the down-state and restoring framework-owned actor state | rescue, release |
| automatic recovery delay | Real-time delay before framework recovery begins | recovery duration, recovery seconds |
| recovery hold | A defeated actor with no automatic recovery deadline | indefinite recovery, owned state |
| defeat lease | Exclusive consumer ownership of a recovery hold, bounded by the watchdog | claim, ownership lease |
| lease holder ID | Stable consumer identifier that owns a defeat lease | owner, consequence name |
| lease timeout | Watchdog duration for a defeat lease | maximum hold, max hold |
| consequence | A selectable player-defeat response defined by JSON | outcome, scenario when referring to the framework record |
| consequence ID | Stable machine identifier used by overrides, cooldowns, logs, and integrations | consequence name |
| consequence display name | Human-readable label that may be changed or localized | ID, name when identity is intended |
| framework settings | Global framework configuration loaded from `SFSE/OSFDefeat/Settings` | validation config |
| eligibility | The yes/no decision about whether an actor pair may enter defeat | validation when referring to this decision |
| condition | A rule that filters consequence eligibility | validation |
| recent hostile attackers | Defeat-time snapshot of recent damage sources that remain live and hostile | participants, cast |
| AI suspension | Framework-owned disabling of an actor's AI processing | pacify |
| captive faction | A configured faction membership used to make a captive non-hostile | AI suspension, pacify |
| capture relationship | Persistent captor-to-captive content relationship | capture record, claim |
| runtime mutation guard | Mutex and epoch used to serialize or invalidate runtime mutations | runtime state |

`victim` remains valid only as a role name where it is paired with `aggressor`, such as consequence conditions and the legacy actor-event callback signature. `captive` and `captor` remain the canonical roles within a capture relationship.

## Public compatibility aliases

The following names are retained so existing compiled Papyrus consumers, version 1/2 JSON, installed configuration, and saves continue to work. They are not canonical authoring names.

| Legacy name | Canonical replacement |
|---|---|
| `RescueActor` | `RecoverActor` |
| `PacifyActor` | `SuspendActorAI` |
| `ReleaseActor` | `ResumeActorAI` |
| `IsPacified` | `IsActorAISuspended` |
| `ClaimDefeat` | `AcquireDefeatLease` |
| `ReleaseClaim` | `ReleaseDefeatLease` |
| `GetClaimOwner` | `GetDefeatLeaseHolder` |
| `GetLastPlayerParticipants` | `GetLastPlayerRecentHostileAttackers` |
| `GetLastPlayerConsequenceName` | `GetLastPlayerConsequenceId` for identity, or the legacy function only when display text is wanted |
| `GetLastPlayerRecoverySeconds` | `GetLastPlayerAutoRecoveryDelaySeconds` plus `IsLastPlayerRecoveryHeld` |
| `SetRecoveryTimer(actor, positive)` | `ScheduleAutomaticRecovery` |
| `SetRecoveryTimer(actor, non-positive)` | `HoldRecovery` |
| `GetRecoverySecondsRemaining` | `GetAutomaticRecoverySecondsRemaining` |
| consequence JSON `name` | `id` and `displayName` in schema v3 |
| `recoverySeconds` | `autoRecoveryDelaySeconds` in schema v3 |
| settings `defaultRecoverySeconds` | `defaultAutoRecoveryDelaySeconds` |
| `holdRecovery` | `recoveryMode: "held"` in schema v3 |
| `maxHoldSeconds` | `leaseTimeoutSeconds` in schema v3 |
| settings `factionPacify` | `applyCaptiveFactionOnPlayerDefeat` |
| settings `participantScanRadius` | `recentHostileAttackerRadius` |
| `SFSE/OSFDefeat/Validation` | `SFSE/OSFDefeat/Settings` |

Schema v1/v2 consequence files, both settings-directory names, and all legacy JSON keys remain readable. When both settings directories exist, legacy `Validation` files load first and explicitly authored canonical `Settings` keys override them. The shipped canonical default is a non-applying template, so installing an update does not erase legacy user choices. Persistence record FourCCs and binary layouts are intentionally unchanged; their old mnemonics are an implementation-history detail, not a migration mechanism.

## Naming rules for new changes

1. Name a type for the concept it stores, not the operation that created it: `ActiveDefeat`, `CaptureRelationshipRecord`, `AISuspensionRecord`.
2. Put units in duration and deadline names: `autoRecoveryDelaySeconds`, `leaseTimeoutSeconds`, `autoRecoverAtDay`, `leaseExpiresAtDay`.
3. Separate stable identity from presentation: `id` is machine-facing and `displayName` is user-facing.
4. Use `recover` only for ending defeat, `resume` only for AI processing, `release` only for relinquishing a lease or capture relationship, and `remove` for faction membership.
5. Keep the module boundary literal: `PlayerDefeat` owns interception and player consequence orchestration; `Recovery` owns actor-generic timers, holds, leases, and the watchdog.
