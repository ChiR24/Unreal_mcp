# PLUGIN SOURCE-CONTRACT TESTS

Vitest tests that read C++/C# **source text** and assert required/forbidden patterns. They never compile Unreal. Every gate here runs in CI (`npm run test:unit`); a regression fails CI.

## SCOPE
65 files in `tests/unit/plugin/`. Each `*contracts.test.ts` (or `*_contracts.test.ts`) file targets one invariant; `plugin-contract-fixtures.ts` holds shared helpers. No subdirectories.

## WHERE TO LOOK
| Contract | Test file | What it enforces |
|----------|-----------|-------------------|
| 250 pure-line ceiling | `source_structure_contracts.test.ts` | Every plugin `.cpp/.cs/.h` ≤ 250 **pure** lines (non-blank, non-`#`/`//`). A 380-line file with many comments still passes. |
| ≤25 files per folder | `source_structure.test.ts` | No folder under `Private/` exceeds 25 source files. Currently at cap: `MCP/Transport`, `Domains/Sequence`, `Foundation`, `MCP/Execute` (25); `MCP/Generated`, `Domains/GAS`, `Domains/AnimationAuthoring` (24). Add a 26th → CI breaks. |
| No split artifacts | `source_structure_contracts.test.ts` | Regex rejects `Common.*`, `Part\d+`, `.incl`, bare `N.` suffixes. |
| Local Mcp* includes resolve | `source_structure_contracts.test.ts` | Every `#include "Mcp..."` (except `.generated.h`) must resolve to an existing source basename. |
| No `UPackage::SavePackage` | `instanced_struct_contracts.test.ts` | Whole-plugin scan; offenders list must be `[]`. Use `McpSafeAssetSave` / `McpSafeLevelSave` / `McpSafeLoadMap` instead. |
| UNDO_EVIDENCE durability | `undo-evidence-durability-contract.test.ts` | Walks the plugin call graph to prove cited UNDO_EVIDENCE handlers cannot reach a package write — catches aliases like `SaveLoadedAssetThrottled` that name-grep misses. |
| Constant-time token compare | `security_contracts.test.ts` | `McpConstantTimeTokenEquals` exists with `Diff |=` (no early exit); BOTH transports (native + WS) must call it. |
| Non-loopback needs token | `security_contracts.test.ts` | Native lifecycle asserts `SECURITY: refusing to bind ... without RequireCapabilityToken` + loopback fallback. WS asserts `DestroyListenSocket()` precedes `return 0;` (no socket leak). |
| No browser-origin WS upgrade | `websocket_origin_contracts.test.ts` | Origin read → rejection (`if (!Origin.IsEmpty())`) → 101 ordering; message `Browser-origin WebSocket requests are not allowed.`, code 4403. |
| No raw Python in logs | `execute_python_diagnostics_contracts.test.ts` | `execute_python begin:` log contains `codeSha256` not `*Code`/`*File` args; `code` field redacted as `<redacted>`. |
| CI gate order + pinning | `workflow_gate_order_contract.test.ts` | `DETERMINISTIC_GATES` array (16 entries) matches `ci.yml` lint job order; all `uses:` are 40-char SHAs; no secrets in `if:`; no Unreal commands in non-opt-in jobs; Node matrix `20.19.x`+`26.x`. |
| Release archive excludes | `release-archive-excludes.test.ts` | `release.yml` + `package-plugin.sh` exclude `Binaries/`, `Intermediate/`, `Saved/`, `.cache/`, `DerivedDataCache/`. |

## CONVENTIONS
- **`countPureLines()`** (`plugin-contract-fixtures.ts`): a "pure" line is non-blank and not a `#`/`//` comment. Use this helper — do not hand-roll line counting.
- **`sliceBetween()`**: extracts source between two marker strings for focused assertion. Use it to scope a contract to the relevant region instead of scanning the whole file.
- Contracts read files via `fs.readFileSync` + regex; they do **not** import plugin code. Keep them text-based so they run without a compiler.
- When moving/renaming a plugin implementation file, update contract path arrays and assertions **only when the intended invariant changed** — a pure rename should not alter a contract's semantics.
- Each contract test is independent; there is no shared setup beyond the fixtures file. Add a new `*contracts.test.ts` for a new invariant rather than extending an existing one.

## ANTI-PATTERNS
- Do not import plugin C++ from these tests — they are text-scanners by design.
- Do not weaken a contract to make CI pass; fix the source instead.
- Do not add a 26th source file to a capped folder (`MCP/Transport`, `Domains/Sequence`, `Foundation`, `MCP/Execute`) — split into a subdirectory first.
- Do not hand-edit `countPureLines` logic; every contract depends on its definition of "pure".
