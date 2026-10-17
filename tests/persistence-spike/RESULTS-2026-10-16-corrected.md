# Slice 0 corrected probe results — 2026-10-16

The corrected reruns are complete. This report supersedes the identity and naming conclusions
in the [version 1 report](RESULTS-2026-10-16.md), which remains historical evidence.
Both fixtures survived cold restart, two normal planet round trips, away-save reloads,
and a 48-local-hour wait. Alias names and journal substitution work. Landed-ship marker
transfers pass before and after reload, including movement, camera, and normal exit.
This passes slice 0's feasibility gate for alias-held references at the two tested sites,
with the rendering caveat and coverage limits below. Minimal Shadows is the next slice.

## Corrections and evidence

- `99159ba`: replace raw dynamic-ID equality with a private per-instance Actor Value
  stamp plus expected base. Four negative controls reject unstamped and wrongly stamped
  instances of the same bases. Creation writes stamps once; inspection never refreshes
  them. Add StoresText to the subject alias so journal substitution resolves its title.
- `df62c7a`: observe alias cell-detach/attach events and unloaded reference 3D. Calling
  natives on the saved origin Cell off-planet produced errors and default zero/false
  values, so those values could not establish detachment.
- `415f784`: stop retaining the generated origin Cell. After saving/reloading away,
  its script handle stayed unbound even after walking back to both visible original
  objects. Version 4 compares their live parent cells with the player's live cell.
  Original-site evidence combines the recorded normal return route, original planet,
  stamped references, and unchanged pack coordinates. It does not assert native Cell
  object identity across unloading.

The final build is `build/persistence-spike/20261016-221207-2383323318654d8980d008726625581e/build.json`.
Papyrus compilation, Spriggit 0.40.1 round-trip correctness, and the binary inventory
verifier passed (4 records, 3 aliases, 2 objectives). The earlier 17 preparation checks
passed; preparation code did not change during these probe corrections.

| Payload | SHA256 |
| --- | --- |
| ESM | `1F686975D357C20A11F814286962420EA4CA3D4D83C4288ED43C31A2976CEFC6` |
| PEX | `FE6AC659AF9B7810009CAFDD7E47E512B0BFC6A7526ADC1380B3F2BF1A089CEF` |

## Runtime checkpoints

Local Starfield 1.16.244.0, MO2 + SFSE, private disposable profiles/saves. Counts are
assertions within checkpoints, not completed scenarios. No fixture was respawned or
restamped to manufacture a persistence pass.

| Check | Authored | Generated |
| --- | --- | --- |
| Identity negative controls | 4/4, version 4 | 4/4, version 3 |
| Baseline / same-process reload | 10/10 each, version 4 | 10/10 each, version 3 |
| Cold restart | 10/10, version 4 | 10/10, version 3 |
| Actor title and journal substitution | Persistence Subject, including after normal return | Persistence Subject, including after normal return |
| Observed departure | 3/3, version 4; both detached, both 3D unloaded | 3/3 in versions 3 and 4; both detached, both 3D unloaded |
| Normal return after away-save reload | 10/10; trip 1, 0.806087 UT hours | 10/10 under version 4; trip 1, 7.318182 UT hours |
| Repeat after 48 local hours away | 10/10; trip 2, 26.372629 elapsed UT hours | 10/10; trip 2, 33.212162 elapsed UT hours |
| Override request / reload | Alias title remains Persistence Subject; scanner and journal correct after reload | Actor and scanner objective retain Persistence Subject before/after reload |
| Ship marker identity / MoveTo with final probe | 3/3 before and after reload; movement, camera, exit hatch work | 3/3 before and after reload; movement, camera, exit hatch work |

Generated trip 1 used the original Jemison Landing Area [Explored] icon, followed by
walking approximately 340 m back to the fixture. Version 3's diagnostic return passed
9/10 checks: only the stale Cell comparison failed. The returned state was saved and
cold-loaded under version 4; CheckReturn then passed all ten checks without Begin.
This is a replay of the same version 3-created fixture, not a claim of a fresh version 4
baseline. Both original identity stamps survived another ID remapping on that restart.

Trip 2 waited twice for 24 local hours on a Gagarin Landing bench, saved/reloaded
there, and used the same Jemison landing icon again. Both objects reattached and
passed all ten return assertions at 19:02:14. The actor and both scanner objective
texts remained present. A separate marker was captured inside the landed Frontier
before walking back to the fixture; this did not modify either site reference.
Ship transfers passed at 19:03:25 and 19:09:43. The first post-reload attempt at
19:06:59 retained a position failure: the console had paused the delayed check, and
the operator walked 2.3 m before it resumed. The repeat closed the console and waited
stationary for all three assertions before checking controls and the normal exit.
The failure remains in the evidence; it is not counted as a passing attempt.

The authored fixture was created fresh under version 4 at New Atlantis. Its original
stamps, 3811903 and 7811903, survived reload and cold restart while its dynamic IDs
changed. The first normal Gagarin departure passed at 19:34:26, followed by an away-save
reload and return through New Atlantis [Explored]. CheckReturn passed at 19:43:45.
The second departure passed at 19:53:47. Two 24-local-hour waits on the Gagarin bench
were followed by save/load there and another return through New Atlantis [Explored].
CheckReturn passed at 20:02:10, with the same stamps and unchanged pack position.
The normal wait UI reports 13 UT hours per 24 local hours on Gagarin; these are 48
local-hour observations, not 48 UT hours or proof that a cell-reset threshold elapsed.
The captured Frontier marker (stamp 11811903) passed all three transfer assertions at
20:15:38 and, after save/load outside the ship, 20:19:22. Movement, camera, and the normal
New Atlantis exit hatch worked after both transfers. The console was closed and the
delayed checks completed before any movement. The final save is spikev4authcomplete.

The subject rendered only partially after the cold load and both returns despite a
successful Is3DLoaded check. Its full body appeared after the override-save reload;
no replacement, enable toggle, or relocation was used to obtain that result. This visual
caveat remains unresolved and is separate from the passing identity, inventory, name,
and objective checks. Production actor promotion must include a visual acceptance check.

The generated seed is the outdoor Ecological Consortium natural-feature area. This does
not cover an enemy building/interior, promotion of an encounter's existing actor, actual
cell reset, arbitrary ship layouts, or weapon instance modifications. The subject is
an inert, ghosted, freshly created surrogate. The pack and subject are held only by
quest aliases; the experiment does not prove persistence of an unreferenced whole site.
Use these findings to proceed with one-slot Minimal Shadows, then validate an existing
aggressor at a generated enemy structure before broadening the supported locations.
Keep actual reset behavior, weapon-instance transfer, and longer nemesis lifetimes as
explicit acceptance work; this probe does not certify them.

## Local evidence index

Runtime directories below are under `build/persistence-spike/runtime/` and remain local,
including captures, input receipts, private-save paths in control.json, and user logs.
Session receipt `passed` means the interactive session closed normally; it is not a
blanket scenario pass. Build receipts deliberately retain `runtimeVerified: false`.

| Session | Evidence |
| --- | --- |
| `20261016-205017-635-Persistence-authored` | Version 2 baseline, reload, corrected journal/scanner title |
| `20261016-210251-505-Persistence-authored` | Version 2 cold restart, Gagarin departure diagnostics and native Cell errors |
| `20261016-211909-738-Persistence-generated` | Version 3 baseline, planted save, reload |
| `20261016-212300-114-Persistence-generated` | Version 3 cold restart, normal departure, away save/reload, original landing icon and walk back; native-return-cell-errors.log |
| `20261016-221302-030-Persistence-generated` | Version 4 replay pass, name/override captures, two 24-hour waits, away reload, trip 2 pass, ship checks; generated-return48-markers.png, generated-ship-reload-stationary.png; closed normally |
| `20261016-231138-519-Persistence-authored` | Fresh version 4 baseline, negative controls, planted save and reload; closed normally |
| `20261016-231612-655-Persistence-authored` | Version 4 cold restart, two Gagarin departures/away reloads/normal returns, two 24-hour waits, override and ship checks; authored-return48-markers.png, authored-return48-pack.png, authored-override-reload-scanner.png, authored-override-reload-journal.png, authored-ship-reload-stationary.png, authored-ship-reload-controls.png, authored-ship-reload-exit.png; closed normally |

Final profile setup index: `build/persistence-spike/setup-v4-20261016-221241/profiles.json`.
Original user seeds and their sidecars remain the authored Save4 and generated Save5
identified in the first report. The final audit passed 102 checks, recorded locally in
`build/persistence-spike/runtime/20261016-corrected-final-integrity.json`: original seeds
and sidecars, private seed copies, all six version 2–4 profile payloads, final source
hashes, source Default profile/user settings, and restored session profile INIs match
their recorded hashes. Temporary runtime helpers are removed, Starfield is stopped,
and the harness runner lock is released. Private profiles and evidence saves are retained.

The final main Papyrus logs contain no probe native-call errors. The generated replay
has the expected warning that version 3's removed originCell variable was skipped.
Other game/mod warnings remain in the archived logs; this is not a clean-log claim for
the entire loaded mod list. Earlier native-cell errors and the generated ship operator
failure remain preserved as diagnostic evidence.
