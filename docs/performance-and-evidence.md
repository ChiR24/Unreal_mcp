# Performance and evidence

Every number on this page is copied from a recorded evidence file under
`.omo/evidence/`. Where no evidence exists, the gap is named rather than
filled.

**How to read this page.** A figure with an evidence path behind it was
measured. A row marked **BLOCKED** was *not* measured — it is not a pending
optimization, an estimate, or a near-miss. Do not quote a BLOCKED row as a
result.

## Retrieval, latency, memory and payload budgets

Source: `.omo/evidence/task-48-pure-unreal-mcp-implementation.json`
(status `PASS`, 13 declared budgets, 13 passed, 0 failed).

Measured in-process against the working tree on Node 22.22.2 / linux-x64,
registry record count 1,335.

| Budget | Observed | Threshold | Direction |
| --- | --- | --- | --- |
| `retrieval.top1Accuracy` | 0.9107 (51/56) | 0.90 | at least |
| `retrieval.topKRecall` | 1.0 | 0.98 | at least |
| `retrieval.guidedRecoveryRate` | 1.0 | 0.97 | at least |
| `retrieval.unavailableFilterRate` | 1.0 | 1 | exactly |
| `retrieval.destructiveFalseAutoSelections` | 0 | 0 | exactly |
| `retrieval.corpusScorerTop1NotBelowBaseline` | 1 | 1 | at least |
| `latency.warmSearchP95Ms` | 17.01 ms | 50 ms | at most |
| `latency.describeP95Ms` | 0.0103 ms | 25 ms | at most |
| `latency.validationP95Ms` | 0.0183 ms | 10 ms | at most |
| `memory.indexBytes` | 2,005,288 B (≈2.005 MB) | 26,214,400 B | at most |
| `payload.searchBytes` | 13,957 B | 32,768 B | at most |
| `payload.describeBytes` | 6,749 B | 65,536 B | at most |
| `payload.medianDescribeUnionRatio` | 0.1338 | 0.5 | at most |

The last row is the concrete payoff of progressive discovery: a median
`describe` response is **13.4% of the union-schema baseline**.

### Limits that travel with those numbers

Stated in the same evidence file's `honestLimitations`, not softened here:

- **Top-1 clears the gate by one case.** 51/56 = 0.9107 against a 0.90
  threshold; 50/56 = 0.8929 would fail. This is not a comfortable pass, and a
  single catalog change could cross back under.
- Five residual top-1 misses remain. Two (`c.C22`, `c.C40`) are declared
  **permanent honest misses**, not deferred work — neither is fixable by the
  ranker without encoding the expected answer.
- **No live Unreal Editor was involved.** All figures are in-process.
- Latency and memory are machine-dependent and are excluded from the
  deterministic hash. Treat them as indicative of this host, not as a
  cross-machine guarantee.
- `registry:check` is **not** in CI, so generated-artifact drift can be
  reintroduced without CI noticing. Run it locally after touching records.

### Model-arm accuracy: BLOCKED

`modelArm.status` is `BLOCKED_EXTERNAL` / `NOT_ENABLED`. The optional external
model arm is opt-in and was not enabled: **no model was contacted and no
model accuracy is claimed.** Any statement about model-selection accuracy for
this build would be unbacked.

## Load, soak and residue

Source: `.omo/evidence/task-51-pure-unreal-mcp-implementation.json`.

### Closed and measured

| Gate | Result |
| --- | --- |
| Node retained RSS, 32 sessions | worst **+9.24 MiB** against a 32 MiB budget |
| Node second-half growth, 32 sessions | worst **+5.66 MiB**, 32 of 32 sessions |
| Sessions started | 32 of 32, zero start failures |
| Requests answered | **1,000 attempted, 1,000 answered**, 1,000 returned the verdict their kind demands |
| Soak | **500 of 500** cycles opened and completed, 0 open-state leaks |
| Soak retained RSS | 3.23 MiB against 32 MiB |
| Process residue | 12 of 12 rounds completed, 0 residue |
| Cleanup receipts | 32 of 32 released, 0 leaked, 0 present after teardown |
| Execute without a bridge | fails closed — 178 `FAILED_CLOSED`, 94 `REFUSED`, 0 `UNEXPECTED` |

The memory gate is falsifiable by construction: a run whose baseline is itself a
peak is scored `INVALID_VACUOUS_BASELINE`, not `PASS`, and a positive control
proves the sampler detects a deliberate 64 MB leak while an idle child stays
flat.

### BLOCKED — not measured

| Claim | Why |
| --- | --- |
| Editor retained RSS ≤ 64 MiB after warm-up | `EDITOR_OWNED_BY_ANOTHER_LANE` — this lane launched no editor, so it measured none. No editor figure is inferred from the Node figure; they measure different processes. |
| Zero residual UObjects and delegates | Observable only from inside a running editor. |
| Native `/mcp` session load and live native accept/reject parity | The native transport is served by the plugin inside an editor; this lane may not start or bind it. |

### Explicitly not proven

- Native accept/reject parity is decided against a **mirror** of the plugin gate
  (its generated header, parsed at run time, plus a source contract on
  `IsBlockedCommand`) — not against the compiled binary.
- 954 of 4,000 differential rows are non-ASCII and were left **undecidable**
  rather than adjudicated. **Parity for non-ASCII console commands is not
  claimed.**
- The auth/session and execute-envelope corpora were generated and their
  distributions recorded, but they are executed only by the native driver
  against a live editor, which is blocked. Their counts are evidence of corpus
  construction, not of server behavior.
- Consent, scope and revision refusals are exercised as corpus shapes and
  through the TypeScript envelope, **not end-to-end through the plugin's
  authorization stack.**
- Execute paths were exercised only in their fail-closed form. No editor
  mutation was performed or attempted.

## Engine certification

Source: `.omo/evidence/task-52/certify-5.7.json`
(sha256 `9b4cdb0c…`, generated 2026-07-27T17:26:39Z).

> **There is no completed multi-engine certification.** One engine has a
> recorded run. Everything else is inventory, not certification.

### Engine roots present on the recording host

| Minor | Identity | Buildable | Runnable |
| --- | --- | --- | --- |
| 5.0 | 5.0.3 | yes | no |
| 5.3 | 5.3.2 | yes | no |
| 5.5 | 5.5.4 | yes | no |
| 5.7 | 5.7.4 | yes | yes |
| 5.8 | 5.8.0 | yes | yes |

**Absent: 5.1, 5.2, 5.4, 5.6.** No certification, testing, or support evidence
exists for those four, and none may be claimed. The plugin's *compatibility
target* is UE 5.0–5.8; a compatibility target is a build scope, not a test
result.

Engine identity is read from `Engine/Build/Build.version` (hashed in the
record), never inferred from a folder name — the record even captures a folder
label that contradicts its contents (`…-preview-1` containing `5.8.0-release`).

### What the UE 5.7 run does record

Verdict: **15 of 16 certification stages passed for UE 5.7.**

- A plugin package built from this tree: 3,071,460 bytes, sha256
  `b4f66d37…`, preserved under `.omo/evidence/task-52/artifacts/`.
- Binary freshness proven `FRESH` — the built `.so` is newer than its newest
  input.
- An owned `UnrealEditor-Cmd` launched against a disposable project under
  `/tmp/opencode`, on uniquely allocated ports.
- Three loopback listeners observed **out-of-band** via `procfs:net-tcp`
  (native, plus two WebSocket ports).
- Complete cleanup: the editor PID gone, all three ports refusing connections,
  and the 1,961-entry workspace removed, each verified by an independent
  observation.
- Positive controls pass: every observation mechanism was shown to see both
  `present` and `absent`, so a "gone" reading is not a broken probe.

### What the UE 5.7 run does NOT record

The record's `clients`, `transcripts` and `claims` arrays are all **empty**.
This run proves the certification *orchestration* — package, launch, observe,
clean up — and does not carry client-transcript evidence of corpus execution.
Do not cite it as live corpus coverage.

The one non-passing stage is recorded in the evidence file; it is a harness
guard outcome, and this page does not restate it as a product result.

> **Certification is in flight.** A concurrent lane owns editors, plugin builds
> and UBT. Re-read `.omo/evidence/task-52/certify-5.7.json` before quoting this
> section; the numbers above describe the file hashed as `9b4cdb0c…`.

## Migration map determinism

Source: `.omo/evidence/task-20-pure-unreal-mcp-implementation.json`
(status `PASS`).

- 1,335 audited legacy occurrences; 1,340 migration entries.
- 1,332 resolve to a live canonical capability (1,327 canonical + 5 aliases).
- 8 explicit typed removals; 0 non-translatable entries; 0 alias conflicts.
- The artifact is Zod-schema-validated and **byte-deterministic**: built twice it
  yields identical JSON, so consumers can hash-match
  (`contentHash 0fb127c9…`).

The published tables derived from this map are
[`migration-reference.generated.md`](migration-reference.generated.md) and
[`action-reference.generated.md`](action-reference.generated.md), both gated by
`npm run registry:check`.

## Gates that are NOT in CI

CI runs, in order: `eslint --max-warnings=0`, `type-check`, `test:unit`,
`manifest:check`, `test:params`, `npm audit --audit-level=moderate`; a second
matrix job adds `build` + `test:smoke`.

Not in CI, and therefore not proven by a green run:
`npm test` (integration — needs a live editor), `registry:check`,
`normalization:check`, `policy:check`, `version:check`, `lint:cpp`,
`lint:csharp`. Plugin packaging runs only when `UNREAL_ENGINE_ROOT` is set.

A green CI badge does not mean the canonical registry is fresh. Run
`registry:check` locally.
