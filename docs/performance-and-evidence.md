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

## Engine support and certification

Sources: `.omo/evidence/task-62-pure-unreal-mcp-implementation.json` (the
per-minor aggregate) and `.omo/evidence/task-63-pure-unreal-mcp-implementation.json`
(the claim decision).

**Decided readiness status: `BLOCKED_EXTERNAL`** (rule `FALLBACK-1`; gating
requirements R1, R2, R4 and R5 unmet; R7 and R8 additionally unmet for the
stronger best-in-class claim). 15 blockers are carried, classified by who can
clear them.

> **One of the nine advertised minors has a passing live record, and that
> record does not cover the current tree.** Everything else below is inventory,
> a compile result, or an explicit blocker. None of it is certification and
> none of it may be quoted as one.

### The nine advertised minors

Engine identity is read from `Engine/Build/Build.version` (hashed in the
record), never inferred from a folder name — the aggregate even captures a
folder label that contradicts its contents (`…-preview-1` containing
`5.8.0-release`).

| Minor | Identity | Root present | Editor built | State | Certified | Next step is owned by |
| --- | --- | --- | --- | --- | --- | --- |
| 5.0 | 5.0.3 | yes | no | `BLOCKED_EXTERNAL` / root-unbuilt | no | operator — compile the editor target for the installed root |
| 5.1 | — | no | no | `BLOCKED_EXTERNAL` / root-absent | no | operator — install the engine, then build its editor target |
| 5.2 | — | no | no | `BLOCKED_EXTERNAL` / root-absent | no | operator — install the engine, then build its editor target |
| 5.3 | 5.3.2 | yes | no | `BLOCKED_EXTERNAL` / root-unbuilt | no | operator — compile the editor target for the installed root |
| 5.4 | — | no | no | `BLOCKED_EXTERNAL` / root-absent | no | operator — install the engine, then build its editor target |
| 5.5 | 5.5.4 | yes | no | `BLOCKED_EXTERNAL` / root-unbuilt | no | operator — compile the editor target for the installed root |
| 5.6 | — | no | no | `BLOCKED_EXTERNAL` / root-absent | no | operator — install the engine, then build its editor target |
| 5.7 | 5.7.4 | yes | yes | `PASS`, but the pass is stale | no | us — re-certify against the current tree |
| 5.8 | 5.8.0-preview-1 | yes | yes | `FAIL` — our plugin does not compile | no | us — this is our defect, nothing external is missing |

**The two blocked subclasses are not the same problem and must not be merged.**
`root-unbuilt` means the engine is installed but its `UnrealEditor-Cmd` was
never compiled, so the remediation is a build on an existing root.
`root-absent` means the engine is not on the host at all, so the remediation is
an install first. In both cases compatibility is **UNKNOWN**: seven of the nine
minors have no plugin compile result of any kind, and "we could not try" is
never "it is broken" — nor is it "it works".

### 5.7.4 — a passing run whose certification is stale

Two independent runs recorded **20 of 20 stages PASSED, 0 FAILED, 0 NOT_REACHED**
(Tasks 52 and 59), with 84 automation requests started and 84 completed, and a
23-pass / 0-fail / 0-blocked driver subset over 24 cases on both the native and
stdio transports.

That certification **predates the current tree**: 139 plugin source files are
newer than the certified binary, and the record's own tree snapshot covers the
harness rather than plugin source
(`certificationDrift.recordedTreeCoversPluginSource = false`).

The plugin at `HEAD` **compiles clean** against 5.7.4 — RunUAT `Result:
Succeeded`, 0 errors, 0 warnings, an empty sync-fidelity diff, 278 files on the
current `GetJsonStringField` accessor and 0 on the retired macros.

> **A clean compile is not a certification.** The compile says the current
> source builds; it says nothing about whether the 20 live stages still pass.
> Re-running those stages end-to-end at `HEAD` is separate, owned work and has
> **not** been done. Do not read the compile result as a re-certification.

### 5.8 Preview 1 — our plugin does not compile

This is not an external blocker. The engine is installed and its editor is
built, so nothing is missing from the host; the defect is ours.

RunUAT exits **6** (`UnrealBuildTool: Failed (OtherCompilationError)`) with
**35 compiler errors** across 20 files. Two upstream API breaks account for all
of them:

| Break | Errors | Opt-out |
| --- | --- | --- |
| `FJsonObject::Values` key type changed from `FString` to `UE::TSharedString<char16_t>` | 29 | Epic ships one, documented as "will be removed" |
| `UUserDefinedEnum::SetEnums` grew from 2 parameters to 5 | 6 | none |

Because 5.8 is advertised and does not build, this is an **open decision for a
human**, not a defect to be quietly absorbed. The options and their measured
costs are recorded in `.omo/evidence/task-64-pure-unreal-mcp-implementation.json`.

### What no engine record covers

- Seven of nine minors have **no plugin compile result at all**.
- The 5.7.4 pass carries live stage evidence, but the aggregate's own
  `clients`/`transcripts`/`claims` arrays for the earlier Task 52 run are
  empty; that run proves certification *orchestration* — package, launch,
  observe, clean up — not corpus coverage.
- No engine, editor, plugin build or RunUAT invocation was performed while
  producing this page. Every engine figure here is re-read from the recorded
  Tasks 56–62 evidence.

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
`registry:check`, `normalization:check`, `manifest:check`, `policy:check`,
`test:params`, `migration:check`, `primitives:check`, `security:check`,
`eval:check`, `version:check`, `workflow:check`,
`npm audit --omit=dev --audit-level=high` (blocking), then
`npm audit --audit-level=moderate` (`continue-on-error`, informational); a
second matrix job adds `build` + `test:smoke`.

The blocking audit is runtime-only at `high`, so it does not prove the tree is
advisory-free: `--omit=dev --audit-level=moderate` exits 1 against this
lockfile today. See
[`security-and-receipts.md`](security-and-receipts.md#a-shipped-dependency-carries-an-advisory).

Not in CI, and therefore not proven by a green run:
`npm test` (integration — needs a live editor), `lint:c`, `lint:cpp`,
`lint:csharp`. Plugin packaging runs only when `UNREAL_ENGINE_ROOT` is set.
