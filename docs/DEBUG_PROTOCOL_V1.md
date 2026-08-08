# Unreal MCP Debugging v1

This fork extends Unreal MCP's automation contract. It does not change the Model Context Protocol specification.

## Components

- The Node sidecar owns debug sessions, jobs, the bounded correlated-event store, and artifact metadata.
- `unreal-mcp-debug-host` runs inside VS Code and uses the supported VS Code debug APIs to host `cppvsdbg`.
- `McpAutomationBridge` negotiates automation protocol v2, captures request-scoped diagnostics, and streams Blueprint/test/trace events.
- `McpDebugRuntime` is a non-Shipping loopback agent that publishes read-only, versioned simulation probe snapshots at no more than 10 Hz and 1 MiB per snapshot.

The sidecar stays responsive when a standalone Unreal target is stopped in the debugger. The native HTTP endpoint remains observation-only; native control reports `DEBUG_SIDECAR_REQUIRED`.

## Startup

1. Build the server with `npm run build`.
2. Install `vscode-extension/artifacts/unreal-mcp-debug-host-0.1.0.vsix` and open the Unreal project folder in VS Code.
3. Build the project target as `DebugGame Editor` so its DLL and PDB timestamps match.
4. Configure the MCP client to run `node <fork>/dist/cli.js` with `UE_PROJECT_PATH` and `UE_ENGINE_PATH`.
5. Call `debug_session` with `action: "start"` and `mode: "standalone_debug"`.

Run `npm run test:debug-smoke` to verify the built stdio server exposes all four debug tools, static debug resources, resource templates, and an offline-safe `list_targets` call.

With the VS Code host active and DebugGame symbols built, run `npm run test:debug-live` for the bounded standalone launch/pause/inspect/step/continue/safety/termination smoke test. It sets `UE_MCP_DEBUG_ALLOW_UNSAFE` only inside the test process and still proves that a terminate call without `unsafe: true` is rejected.

The VS Code host writes authenticated discovery metadata to `<Project>/Saved/McpDebug/debug-host.json`. Runtime tokens are random, per-launch, single-use, and accepted only over loopback.

## Tools

- `debug_session`: list/start/status/pause/continue/step/stop sessions.
- `debug_breakpoint`: upsert/remove/list/clear source, function, exception, and log breakpoints.
- `debug_inspect`: threads, stack, scopes, variables, evaluate, read memory, and stopped-state snapshots.
- `debug_observe`: events, Blueprint diagnostics, probe snapshots, recordings, tests, traces, and bundles.

Tests and trace operations return jobs. Poll `test_status` or `trace_status`, or read `ue://debug/jobs/{jobId}`. A completed operation is one of `passed`, `failed`, `cancelled`, or `timed_out`; accepting a request is never reported as a completed success.

## Resources

- `ue://debug/sessions`
- `ue://debug/session/{sessionId}`
- `ue://debug/events/{sessionId}?after={cursor}&limit={limit}`
- `ue://debug/jobs/{jobId}`
- `ue://debug/artifacts/{artifactId}`
- `ue://debug/health`

Artifact resources contain an absolute path, byte size, and SHA-256 digest. Large files are not embedded in MCP context. Files are retained beneath `<Project>/Saved/McpDebug/<sessionId>/`; health reports a warning after the artifact tree reaches 5 GiB.

## Safety

Read-only inspection and ordinary breakpoint/control operations are enabled by default. Assignment expressions, function calls, target termination, and future write/delete operations require both:

```text
UE_MCP_DEBUG_ALLOW_UNSAFE=true
unsafe: true
```

Attach is limited to processes launched by the debug host for the currently open Unreal project. `McpDebugRuntime` is excluded from Shipping targets.

## Protocol v2

The bridge hello offers `[2, 1]` and requests structured diagnostics, correlated events, asynchronous jobs, Blueprint diagnostics, and runtime probes. A peer that cannot select v2 uses the legacy v1 contract.

Every v2 automation event has a monotonic sequence, timestamp, and correlation context. Failures retain the legacy string error and add a stable diagnostic code, severity, component, phase, retry policy, recovery hints, and artifact references. A synchronous handler response is buffered until its scoped Unreal diagnostics close, preventing an engine error or ensure from being reported as success.

This instrumentation supports engineering validation through deterministic replay evidence, state measurements, and error correlation. It does not make a blanket “100% accurate physics” guarantee; model fidelity must be demonstrated against the vehicle, sensors, boundary conditions, and measured ground truth.
