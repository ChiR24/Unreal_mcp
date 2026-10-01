# PLUGIN SOURCE-CONTRACT TESTS

Vitest tests that read C++/C# **source text** and assert required or forbidden patterns. They never compile Unreal and run in CI (`npm run test:unit`). Only structural rules and security wiring are gated here; implementation spelling is not.

## WHERE TO LOOK
| Contract | Test file | What it enforces |
|----------|-----------|-------------------|
| 250 pure-line ceiling, no split artifacts, local includes resolve | `source_structure_contracts.test.ts` | Every plugin `.cpp/.cs/.h` ≤ 250 **pure** lines (non-blank, non-`#`/`//`); rejects `Common.*`, `Part\d+`, `.incl`; every `#include "Mcp..."` resolves |
| ≤25 source files per folder | `source_structure.test.ts` | Any folder under `Private/`. At cap (25): `MCP/Gateway`, `Foundation`, `Domains/Sequence`, `Domains/ControlEditor`; at 24: `MCP/Transport`, `MCP/Generated`, `MCP/Execute`, `Domains/ControlActor`, `Domains/AnimationAuthoring` |
| Includes are self-sufficient | `source_include_self_sufficiency.test.ts` | Users of shared JSON helpers include a header that declares them (no unity-blob borrowing) |
| Token, bind, socket, SavePackage | `security_contracts.test.ts` | Constant-time token compare on both transports; non-loopback bind refused without `RequireCapabilityToken`; replies never reach an unrelated socket; import sources stay inside the project; no `UPackage::SavePackage` outside the Safety wrappers |
| Pre-queue gate | `prequeue-gate-contracts.test.ts` | Both transports authorize before enqueueing; the gate and the dispatcher resolve the same action |
| Browser-origin WS upgrade | `websocket_origin_contracts.test.ts` | Origin rejected before the 101 (code 4403) |
| Reflection targets | `reflection_target_contracts.test.ts` | get/set_object_property refuse `/Script` targets (outermost package) before reading or writing, and report `OBJECT_NOT_ADDRESSABLE` |
| Response identity redaction | `response_identity_redaction_contracts.test.ts` | Identity keys are redacted before a reply leaves; console_command output and the launch_build log tail go through the per-line sanitizer |
| Fab bridge | `fab_bridge_security_contracts.test.ts` | No arbitrary script reaches the page; credentials never reach a reply or a log |
| Fab page scripts | `fab_add_script_behaviour.test.ts`, `fab_details_script_behaviour.test.ts`, `fab_search_script_behaviour.test.ts` (with `fab-page-script.ts`) | The page JavaScript is lifted out of the C++ raw literals and run in a `vm` against a scripted `fetch`: format, tier and engine-build selection, reply shape, refusals, and that no token or signed URL is reported |
| Fab imports | `fab_import_operations_contracts.test.ts`, `fab_import_cancel_contracts.test.ts`, `fab_library_sync_contracts.test.ts`, `fab_listing_details_contracts.test.ts`, `fab_relocate_contracts.test.ts`, `fab_page_readiness_contracts.test.ts` | The add answers when Fab accepts, one import runs at a time, a cancel is only Fab's own button or Interchange's own cancel, an empty library is synced with one fixed command, a relocation touches only what the import created in its own folders and is all or nothing |
| Python diagnostics | `execute_python_diagnostics_contracts.test.ts` | Logs carry `codeSha256`, never the code |
| Behaviour recipe files | `behaviour_recipes.test.ts` | Every `Resources/Recipes/<Domain>/<Name>.json` uses only the fields the plugin accepts, fills every placeholder it declares, calls no Blueprint-dependent function without `memberClass`, and ships (FilterPlugin.ini keeps `/Resources`) |

## CONVENTIONS
- Use `countPureLines()` and `sliceBetween()` from `plugin-contract-fixtures.ts`; do not hand-roll them.
- Tests read files with `fs.readFileSync` + regex; they never import plugin code.
- Add a test here only for a structural or security invariant. Behaviour belongs in an integration case (`tests/mcp-tools/`).

## ANTI-PATTERNS
- Weakening a security contract to make CI pass; fix the source.
- Adding a 26th source file to a capped folder; split into a subdirectory first.
- Pinning implementation spelling (exact log strings, symbol names) as a "contract".
