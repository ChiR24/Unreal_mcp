/// <reference types="node" />

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

// Task 30 Wave 1 — native single-tool cutover, RED source-contract lane.
//
// Task 30 removes ONLY the native public gateway compatibility switch
// (bEnableNativeGateway -> bGatewayMode) and the legacy direct-listing /
// direct-call code path. The private 23-parent dispatch inside `unreal.execute`
// (FMcpToolRegistry, StreamToolCall, HandleGatewayExecute, TryHandleLocalToolCall,
// the generated parent registry) is preserved and deliberately NOT asserted
// here — this lane pins the removed public surface, not the private engine.
//
// A live-editor HTTP harness is not available in CI and the serialized UE
// BuildPlugin gate is the authoritative compile proof, so these read the plugin
// C++ source and pin the post-cutover shape. They are RED until Task 30 lands,
// because every branch below still keys off the gateway-mode toggle today.

const moduleRoot = resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge');
const read = (rel: string): string => readFileSync(resolve(moduleRoot, rel), 'utf8');

const settings = read('Public/McpAutomationBridgeSettings.h');
const header = read('Private/MCP/Transport/McpNativeTransport.h');
const discovery = read('Private/MCP/Transport/McpNativeTransportToolDiscovery.cpp');
const sessions = read('Private/MCP/Transport/McpNativeTransportSessions.cpp');
const jsonRpc = read('Private/MCP/Transport/McpNativeTransportJsonRpc.cpp');
const lifecycle = read('Private/MCP/Transport/McpNativeTransportLifecycle.cpp');
const gateway = read('Private/MCP/Transport/McpNativeTransportGateway.cpp');
const subsystemLifecycle = read('Private/Core/Subsystem/McpAutomationBridgeSubsystemLifecycle.cpp');

// Slice a top-level function body from its signature to its column-0 closing
// brace. Internal braces are always indented, so the first "\n}" is the close.
// Robust against neighbour reordering/deletion and internal reformatting.
const fnBody = (source: string, signature: string): string => {
  const start = source.indexOf(signature);
  if (start < 0) return '';
  const end = source.indexOf('\n}', start);
  return end < 0 ? source.slice(start) : source.slice(start, end + 2);
};

describe('Task 30 cutover: the public native gateway switch is gone', () => {
  it('drops bEnableNativeGateway and its opt-out UI metadata from settings', () => {
    expect(settings).not.toContain('bEnableNativeGateway');
    expect(settings).not.toContain('Enable Native Gateway Mode');
    // The neighbouring native-MCP settings must survive the removal.
    expect(settings).toContain('bEnableNativeMCP');
    expect(settings).toContain('bLoadAllToolsOnStart');
  });

  it('removes the bGatewayMode member and the Start gateway parameter from the transport header', () => {
    expect(header).not.toContain('bGatewayMode');
    expect(header).not.toContain('bInEnableGateway');
    // Start still exists; only its gateway toggle is gone.
    expect(header).toContain('bool Start(int32 Port');
  });

  it('drops the gateway parameter and its assignment from transport Start()', () => {
    const start = fnBody(lifecycle, 'bool FMcpNativeTransport::Start(');
    expect(start).not.toContain('bInEnableGateway');
    expect(start).not.toContain('bGatewayMode');
  });

  it('stops the subsystem from reading the gateway setting when starting the transport', () => {
    const startNative = fnBody(
      subsystemLifecycle,
      'void UMcpAutomationBridgeSubsystem::StartNativeTransport()',
    );
    expect(startNative).not.toContain('bEnableNativeGateway');
    // The preserved trailing Start() argument pins that only the gateway arg was removed.
    expect(startNative).toContain('Settings->bAllowNonLoopback');
  });
});

describe('Task 30 cutover: tools/list is a permanent single-tool listing', () => {
  it('makes HandleToolsList unconditionally list only the unreal tool', () => {
    const body = fnBody(discovery, 'FString FMcpNativeTransport::HandleToolsList(');
    // The legacy gateway-mode fork and the dynamic filtered listing are gone.
    expect(body).not.toContain('if (bGatewayMode)');
    expect(body).not.toContain('GetFilteredToolsResponse');
    expect(body).not.toContain('GetEnabledToolNames');
    // The static one-tool listing is the only path left.
    expect(body).toContain('BuildUnrealGatewayToolDefinition()');
  });

  it('never advertises tools.listChanged in the initialize capabilities', () => {
    // The static single tool never changes shape, so listChanged is omitted, not
    // advertised true/false. Assert on the code that set it, not on comments.
    expect(discovery).not.toContain('SetBoolField(TEXT("listChanged")');
    expect(discovery).not.toContain('bGatewayMode');
    // The tools capability object itself is still advertised.
    expect(discovery).toContain('SetObjectField(TEXT("tools")');
  });

  it('permanently suppresses list-changed notifications in OnToolsListChanged', () => {
    const body = fnBody(sessions, 'void FMcpNativeTransport::OnToolsListChanged()');
    // No gateway-mode gate and no reachable broadcast: suppression is unconditional.
    expect(body).not.toContain('bGatewayMode');
    expect(body).not.toContain('BroadcastToolsListChanged();');
  });
});

describe('Task 30 cutover: pre-dispatch is unconditional with an executable migration', () => {
  it('runs pre-dispatch for every tools/call so the legacy direct-call tail is dead', () => {
    const call = fnBody(jsonRpc, 'void FMcpNativeTransport::HandleToolsCall(');
    // The bGatewayMode guard is gone; pre-dispatch runs for every tools/call.
    expect(call).not.toContain('bGatewayMode');
    expect(call).toMatch(/if \(\s*HandleGatewayModePreDispatch\(/);
    // Pre-dispatch stays total (never returns false: unreal -> gateway, else ->
    // migration payload), so nothing after "if (pre-dispatch) return;" executes.
    const pre = fnBody(gateway, 'bool FMcpNativeTransport::HandleGatewayModePreDispatch(');
    expect(pre).not.toContain('return false');
    expect(pre).toContain('if (ToolName == TEXT("unreal"))');
  });

  it('replaces the bare rejection with a delegated bounded migration payload', () => {
    const pre = fnBody(gateway, 'bool FMcpNativeTransport::HandleGatewayModePreDispatch(');
    // The old plain-text rejection is gone.
    expect(pre).not.toContain("Gateway mode is enabled. Call the 'unreal' tool");
    // Pre-dispatch delegates to the shared, testable migration builder and wraps
    // the receipt in a typed tool-result with the stable removed-tool error code.
    expect(pre).toContain('McpBuildDirectCallMigration(');
    expect(pre).toContain('DIRECT_TOOL_CALL_REMOVED');
    // The authoritative known-parent set comes from the registry, not a local list.
    expect(pre).toContain('GetToolNames()');
  });
});

// Task 30 migration-fix — the direct-call migration must mirror the TS
// buildDirectCallMigration 3-way branch exactly (unknown -> search + bounded
// suggestions, known/no-action -> describe, known/action|subAction -> execute
// with stripped params), on a flat receipt keyed by DIRECT_TOOL_CALL_REMOVED.
// Read lazily so a not-yet-created builder fails only these tests (RED), never
// the cutover invariants above.
describe('Task 30 cutover: the direct-call migration is a bounded 3-way receipt', () => {
  const migrationSource = (): string =>
    read('Private/MCP/Gateway/McpNativeGatewayDirectCallMigration.cpp');
  const migrationSignature = 'TSharedPtr<FJsonObject> McpBuildDirectCallMigration(';

  it('keys the receipt on a flat tool + stable DIRECT_TOOL_CALL_REMOVED, never a nested removedTool', () => {
    const migration = migrationSource();
    // Flat top-level keys mirror the TS DirectCallMigrationResult; the pre-review
    // guidance.removedTool wrapper is gone.
    expect(migration).toContain('SetStringField(TEXT("tool"), ToolName)');
    expect(migration).toContain('TEXT("DIRECT_TOOL_CALL_REMOVED")');
    expect(migration).toContain('SetObjectField(TEXT("nextCall")');
    expect(migration).not.toContain('removedTool');
  });

  it('copies nextCall.operation into a top-level operation so the flat receipt matches the unreal output schema', () => {
    // The registered `unreal` output schema (and the TS DirectCallMigrationResult)
    // require a top-level `operation` beside success:false; the shared receipt
    // builder copies it from the branch's nextCall so the flat receipt and the
    // executable nextCall step can never disagree.
    const receipt = fnBody(migrationSource(), 'TSharedPtr<FJsonObject> MigrationReceipt(');
    expect(receipt).toContain('SetBoolField(TEXT("success"), false)');
    expect(receipt).toContain('NextCall->TryGetStringField(TEXT("operation")');
    expect(receipt).toContain('SetStringField(TEXT("operation")');
  });

  it('routes an unknown tool to search with bounded closest-match suggestions', () => {
    const body = fnBody(migrationSource(), migrationSignature);
    // Unknown = not in the authoritative parent set -> search, never describe/execute.
    expect(body).toContain('!ParentNames.Contains(ToolName)');
    expect(body).toContain('TEXT("search")');
    expect(body).toContain('GatewayClosestMatches(ToolName, ParentNames');
    expect(body).toContain('SetArrayField(TEXT("suggestions")');
  });

  it('bounds suggestions to the shared MAX_SUGGESTIONS ceiling of 3', () => {
    expect(migrationSource()).toContain('McpMigrationMaxSuggestions = 3');
  });

  it('orders the branches unknown->search, known-no-action->describe, known-action->execute', () => {
    const body = fnBody(migrationSource(), migrationSignature);
    const search = body.indexOf('TEXT("search")');
    const describe = body.indexOf('TEXT("describe")');
    const execute = body.indexOf('TEXT("execute")');
    // A total order proves no unknown-describe and no actionless-execute: every
    // earlier branch returns before the next can run.
    expect(search).toBeGreaterThan(-1);
    expect(describe).toBeGreaterThan(search);
    expect(execute).toBeGreaterThan(describe);
  });

  it('selects the action with getString parity: string-typed, trimmed, non-empty, action-then-subAction', () => {
    const migration = migrationSource();
    // Mirrors TS getString(args,'action') ?? getString(args,'subAction'). Each
    // assertion pins a behavioral case the pre-fix "accept any string" code failed:
    //   {action:''}                    -> reject empty-after-trim -> describe
    //   {action:'  create  '}          -> TrimStartAndEnd -> execute action 'create'
    //   {action:'',subAction:'create'} -> empty action falls through to subAction
    //   {action:<non-string>}          -> string-typed only -> falls through
    const helper = fnBody(migration, 'bool GetTrimmedStringArg(');
    expect(helper).toContain('Value->Type != EJson::String');
    expect(helper).toContain('TrimStartAndEnd()');
    expect(helper).toMatch(/if \(Trimmed\.IsEmpty\(\)\)/);
    // On a hit the TRIMMED value is what wins, so '  create  ' resolves to 'create'.
    expect(helper).toContain('OutValue = Trimmed;');
    const body = fnBody(migration, migrationSignature);
    expect(body).toContain('GetTrimmedStringArg(Arguments, TEXT("action"), Action)');
    expect(body).toContain('GetTrimmedStringArg(Arguments, TEXT("subAction"), Action)');
    // action wins when present; an empty/whitespace/non-string action falls through
    // to subAction via the || short-circuit (TS getString(a) ?? getString(b)).
    expect(body).toMatch(
      /GetTrimmedStringArg\(Arguments, TEXT\("action"\), Action\)\s*\|\|\s*GetTrimmedStringArg\(Arguments, TEXT\("subAction"\), Action\)/,
    );
    // The pre-fix shape that accepted any string (incl. empty/whitespace) is gone.
    expect(body).not.toContain('ActionValue->AsString()');
    // Execute wiring still carries the resolved action + migrated params.
    expect(body).toContain('SetStringField(TEXT("operation"), TEXT("execute"))');
    expect(body).toContain('SetStringField(TEXT("action"), Action)');
    expect(body).toContain('SetObjectField(TEXT("params")');
  });

  it('merges nested params under top-level and strips routing/control fields without mutating input', () => {
    const merge = fnBody(migrationSource(), 'TSharedPtr<FJsonObject> MigratedParams(');
    // Nested params first, then top-level keys win (mirrors TS {...nested, ...args}).
    expect(merge).toContain('TryGetObjectField(TEXT("params")');
    expect(merge).toMatch(/Merged->Values\s*=\s*\(\*Nested\)->Values;/);
    // Routing/control fields never leak into the executable params.
    expect(merge).toContain('RemoveField(TEXT("action"))');
    expect(merge).toContain('RemoveField(TEXT("subAction"))');
    expect(merge).toContain('RemoveField(TEXT("params"))');
    expect(merge).toContain('RemoveField(TEXT("operation"))');
    // A fresh object is built; the caller's Arguments is only read, never written.
    expect(merge).toContain('MakeShared<FJsonObject>()');
    expect(merge).not.toContain('Arguments->SetField');
    expect(merge).not.toContain('Arguments->RemoveField');
  });
});
