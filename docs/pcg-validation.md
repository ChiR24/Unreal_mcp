# PCG validation

This guide describes the checked-in commands for PCG source/integration tests and the manual handoff needed by the selected-owner cold-load suite. Live suites create, save, generate, or delete assets. Run them only against an explicitly selected isolated Unreal project that you own; never point their cleanup at a production editor.

The test runner verifies the connected editor's project name, project directory, and optional engine-version prefix before fixture mutations. If the identity check fails, it aborts before running the cases. Set the expected identity from the editor you intentionally started, not from a remembered process or old report.

## Offline checks

These checks do not need Unreal Editor:

```sh
npm run test:unit
npm run test:params
npm run build
npm run test:smoke
```

`test:smoke` uses the built `dist/` output and an in-memory MCP transport. It checks the public gateway surface, not PCG execution in Unreal. `npm test` is also editor-dependent; the PCG suites below are more specific.

## Live editor environment

The live integration runner uses the TypeScript stdio server and its WebSocket connection to the editor by default. Build the server first; the runner can also build stale `dist/` automatically. Do not enable TypeScript fallback when the purpose is to validate the built package.

In PowerShell, set the expected project identity and the WebSocket ports configured for that editor:

```powershell
$env:UNREAL_MCP_TEST_PROJECT_NAME = "<expected isolated project name>"
$env:UE_PROJECT_PATH = "<absolute isolated .uproject path or project directory>"
$env:UE_EDITOR_BASE_DIR = "<absolute base used to resolve a relative projectDir response>"
$env:UNREAL_MCP_TEST_ENGINE_VERSION = "<optional engine-version prefix>"
$env:MCP_AUTOMATION_WS_PORTS = "<configured WebSocket port list>"
$env:UNREAL_MCP_TEST_THROTTLE_MS = "1000"

npm run build
node tests/mcp-tools/world/manage-pcg-reliability.test.mjs
```

`UE_EDITOR_BASE_DIR` is needed only when the editor reports a relative `projectDir`. `UNREAL_MCP_TEST_ENGINE_VERSION` is optional; when set, the runner requires the connected engine version to begin with that prefix. The port list must match the selected editor. Dense suites should use a 1,000 ms inter-case throttle instead of increasing the editor's quotas.

Run one live suite at a time. Each suite can create or change fixtures and shares the selected editor connection. For example, the checked-in PCG suites include:

```powershell
node tests/mcp-tools/world/manage-pcg-reliability.test.mjs
node tests/mcp-tools/world/manage-pcg-parameters.test.mjs
node tests/mcp-tools/world/manage-pcg-execution.test.mjs
node tests/mcp-tools/world/manage-pcg-lifecycle.test.mjs
node tests/mcp-tools/world/manage-pcg-data.test.mjs
node tests/mcp-tools/world/manage-pcg-data-param.test.mjs
node tests/mcp-tools/world/manage-pcg-data-spatial.test.mjs
node tests/mcp-tools/world/manage-pcg-instance-schema.test.mjs
node tests/mcp-tools/world/manage-pcg-spawner-types.test.mjs
node tests/mcp-tools/world/manage-pcg-flow.test.mjs
node tests/mcp-tools/world/manage-pcg-switch.test.mjs
node tests/mcp-tools/world/manage-pcg-workflow.test.mjs
node tests/mcp-tools/world/manage-pcg-component-setup.test.mjs
```

The `manage-pcg-component-save` and `manage-pcg-component-cold-load` suites form a separate persistence workflow described below. Do not run a cold suite without its saved fixture manifest. Some suites use Python Editor Script Plugin for fixture operations; a structured workflow that does not use Python does not imply that every suite has the same fixture prerequisites.

The normal integration runner writes test-result JSON under `tests/reports/`; it does not retain the runner's capture map in that JSON. Values needed by a later suite are printed as `Captured: ...` lines in the test process output. Keep reports and raw logs local; they can include project-specific paths and fixture names. A green TypeScript test, build, or mock smoke is not a native C++ build or live-editor result.

The checked-in runner's default route validates stdio/WebSocket. A WebSocket pass is not a native HTTP/SSE pass. To run a suite through the checked-in native proxy instead, keep the same isolated-project variables and the editor's WebSocket readiness ports, enable that editor's native endpoint, and set:

```powershell
$env:UNREAL_MCP_SERVER_CMD = "node"
$env:UNREAL_MCP_SERVER_ARGS = "$((Resolve-Path 'dist/cli.js').Path),proxy"
$env:UNREAL_MCP_URL = "http://127.0.0.1:<configured native port>/mcp"
$env:UNREAL_MCP_TEST_THROTTLE_MS = "1500"
node tests/mcp-tools/world/manage-pcg-component-setup.test.mjs
```

Replace the native-port placeholder with that editor's actual loopback port. The proxy uses the selected project's normal token lookup; do not put tokens in commands or reports. These overrides route the suite's MCP requests over native HTTP/SSE even though the runner also checks editor readiness on the configured bridge ports. Record native and WebSocket runs separately. Clear `UNREAL_MCP_SERVER_CMD`, `UNREAL_MCP_SERVER_ARGS`, and `UNREAL_MCP_URL` before returning to the default WebSocket route.

## Create a cold-load manifest without local helpers

The component save/cold-load suites do not require an ignored deployment helper to exchange fixture identity. The save suite creates its own uniquely named non-partitioned Map, Graph, actor, and two native components. It saves only its selected Graph and Map, verifies the components are idle and empty, and leaves the owned fixture for the separate cold suite. It does not write the cold suite's manifest for you.

1. Run `node tests/mcp-tools/world/manage-pcg-component-save.test.mjs` in the selected isolated project using the environment above.
2. Save the process output long enough to copy the `Captured:` values for `graphPath`, `mapPath`, `actorName`, `componentAName`, `componentBName`, `componentAPath`, `componentBPath`, and `pointNodeId`. The `runRoot` is the unique parent directory containing the captured `Graph` asset path. Use exact values; do not reconstruct component object paths from labels.
3. Create a UTF-8 JSON file, without a byte-order mark, under the selected project’s `Saved/PCGValidation` directory. It must be a regular file, under 64 KiB, with exactly these nine string fields and no additional metadata:

   ```json
   {
     "runRoot": "/Game/MCP_PCG_Validation/ComponentSave_<unique-id>",
     "mapPath": "/Game/MCP_PCG_Validation/ComponentSave_<unique-id>/Map",
     "graphPath": "/Game/MCP_PCG_Validation/ComponentSave_<unique-id>/Graph",
     "actorName": "<captured actorName>",
     "componentAName": "<captured componentAName>",
     "componentBName": "<captured componentBName>",
     "componentAPath": "<captured componentAPath>",
     "componentBPath": "<captured componentBPath>",
     "pointNodeId": "<captured pointNodeId>"
   }
   ```

   Replace every placeholder with the exact value from that run before saving. `runRoot` must be one unique `/Game/MCP_PCG_Validation/ComponentSave_<id>` folder; `Map` and `Graph` must be its assets. Both component paths must identify distinct components in that Map and end in their exact component names. Names and node IDs must match the test's safe-character rules. Do not add hashes, timestamps, report paths, credentials, or extra fields to this fixture manifest.
4. Confirm the saved fixture is idle and its selected Map is clean; preserve unexpected dirty state rather than saving or discarding unrelated packages. Normally exit the save-run editor. Start a new editor process for the same selected isolated project and plugin on a different, clean map, not the fixture Map: the cold suite requires a real load rather than an already-open no-op. Before making a cold-load claim, independently verify the different process and intended project/plugin identities. The manifest is a fixture locator, not provenance evidence.
5. Set the manifest environment variable to the absolute JSON file path, then run the cold suite in the new editor process:

   ```powershell
   $env:UNREAL_MCP_PCG_COMPONENT_PERSISTENCE_MANIFEST = "<absolute path under the selected project's Saved/PCGValidation>"
   node tests/mcp-tools/world/manage-pcg-component-cold-load.test.mjs
   ```

   Keep the selected project identity variables set in this process as well. The cold suite checks saved component bindings, manual flags, seeds, graph data, selected generation, typed point output, and owned cleanup. It does not itself compare the warm and cold process IDs, calculate a source/DLL/hash index, or prove which binary was loaded in the earlier process. Record those provenance checks separately if they are part of the acceptance claim.

A fixture that cannot be proven idle or owned must be preserved, not broadly deleted. Never use save-all or discard-unsaved as a cleanup fallback. Check the latest exported test report and the actual editor state; a process exit code alone is not enough.

## Native closeout checks

After building the plugin in the selected isolated editor project, run the registered `EngineWire.PCG.Closeout.` tests through `unreal` → `system_control.run_tests` and also run `EngineWire.PCG.Execution.`. Require the expected tests to be matched and passed, with no failures or not-run entries; an empty match is not a pass. These cover observer teardown, inventory limits, native struct identity, traversal/offset bounds, existing retention/resource sampling, and compatible native integer node positions. Source-contract tests inspect text and do not execute these C++ tests.

The original closeout changes to observer teardown, managed-inventory bounds, parameter schema and traversal/pagination passed selected native batches of 63/63 on UE 5.7.4 and 64/64 on UE 5.8.3. Their public matrix had 15 complete passing runs and one UE 5.8 native selector/packer failure: 43 passed, 1 failed, stopped after 44 of 76. The original reports and diagnostic remain historical evidence, not silently relabelled passes.

That failure was traced to an autosave of the exact graph between root listing and typed-view creation. Saving assigned localization identities to generated pin-tooltip format arguments without changing their source text. Both the graph-read projection and the reflected pin-state fingerprint included those text histories, so the old root correctly encountered a different digest even though no authored input had changed. A deterministic native regression failed on the old implementation for this identity-only change before the fix was applied.

Revision capture now compares culture-independent pin-tooltip source text in its private hash state. Public property exports and strict data-view revision validation are unchanged. Real tooltip source, pin restrictions and settings changes still invalidate old views; invariant/localizable kinds and direct string-table entry identity remain distinct.

After integration with upstream `dev` (package version `0.6.0-beta-f` and MCP SDK v2), the selected native batches passed **68/68 on UE 5.7.4** and **69/69 on UE 5.8.3**. Four pin-text regressions cover identity promotion, culture/text-kind controls, retained projection validity and an actual selected-graph safe save. The save test requires the native text export to change during persistence, then reads both pre-save and post-save typed views through the same pre-save root handle. An additional compatibility regression verifies that an existing node's `nodeName` locator preserves its authored title and that an empty edit is refused. Separate existing authored pin/subgraph/parameter controls passed 3/3 and 4/4, respectively. Neither result is a whole-plugin native certification.

The affected public workflows were then freshly exercised on those integrated binaries:

| Workflow | UE 5.7 WebSocket / native | UE 5.8 WebSocket / native |
| --- | --- | --- |
| Selector/packer generation and readback | 76/76 / 76/76 | 76/76 / 76/76 |
| Point/Data reads | 77/77 / 77/77 | 77/77 / 77/77 |

**The autosave/pin-tooltip revision blocker is resolved within this tested scope; all eight affected public runs passed.** This conclusion includes old-code reproduction and real-save regression evidence, not just a fresh green rerun. Run-owned fixtures were precisely cleaned and both isolated editors exited normally.

The earlier isolated placed-component setup (44/44) and typed graph-parameter/override workflows (116/116) passed on both transports on both closeout engines. They were not rerun for this revision-only fix and are not acceptance claims for its new binaries. Earlier persistence evidence likewise remains tied to its original build. Other unresolved incidents and untested modes below are not automatically closed by this fix.

## Interpreting the bounded evidence

Before those closeout changes, selected PCG editor CPU workflows have been exercised on Windows UE 5.7.4 and 5.8.3 over WebSocket and native HTTP/SSE, including Point/Data, Param data-domain, bounded Spatial root/schema/refusal, schema/selector, Branch/Switch, and selected-owner persistence slices. The component save and real-restart cold-load suites recorded 38/38 and 33/33 cases, respectively, per transport per engine. These are bounded slices, not full PCG certification. The default checked-in runner command above covers WebSocket only.

The component setup suite requires an explicit isolated-project identity. Older 44-case reports omitted that option and ran the generic fixture preamble; those historical assertions are not clean-isolated acceptance. The corrected suite passed on both transports on both tested engines in the original closeout matrix described above.

Known limits and open validation:

- Four earlier graph/component revision or identity causes remain unresolved. Separate prior Point/Data failures are preserved; later bounded passes did not establish their cause or a general fix.
- UE 5.8 Maps were observed dirty after the public cold phase. The cause is unknown. A selected actor-free Map save was separately verified; no save-all/discard fallback was used. This is not evidence of a graph-deletion defect.
- Six unrelated full-plugin native test failures have historically reproduced on the original baseline with matching assertions. They were not fixed here; a selected PCG native pass is not a green whole-plugin native run.
- Exact scoped-profile Read/Write and foreign-principal live acceptance were not run. A structured graph-instance asset factory/rebind workflow is not provided by the common parameter actions.
- GPU, PIE/runtime, World Partition/HiGen, Mac/Linux, Node 20/26 validation, a genuine missing-PCGEditor installation, and full whole-plugin native acceptance are not established by the selected CPU editor workflows. The native matrix uses the initialized HTTP/SSE path; it does not separately certify sessionless `2026-07-28` PCG requests.
- Arbitrary user Blueprint callback side effects are not sandboxed. A staging copy is not proof of callback isolation.

For generation, data-view, revision, permission, and compatibility semantics, see [PCG authoring and execution](pcg-authoring.md).