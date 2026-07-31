/// <reference types="node" />

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

// Source-contract lane for plan Task 28, "Make capability advertisement and
// catalog change semantics truthful", on the native /mcp surface.
//
// Two independently runnable groups live in this file:
//   * "Task 28 BASELINE" pins the behavior that already holds on this tree and
//     must survive the Task 28 change. Run it alone with
//     `npx vitest run tests/unit/plugin/native-capability-state-contracts.test.ts -t BASELINE`.
//   * "Task 28 DESIRED" encodes the native behavior that does NOT exist yet and
//     is therefore deliberately RED until Task 28 is implemented.
//
// These read the plugin C++ source because no live-editor HTTP harness runs in
// CI; the serialized UE BuildPlugin gate remains the authoritative compile proof.

const pluginRoot = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private',
);

const read = (rel: string): string => readFileSync(resolve(pluginRoot, rel), 'utf8');

const discovery = read('MCP/Transport/McpNativeTransportToolDiscovery.cpp');
const sessions = read('MCP/Transport/McpNativeTransportSessions.cpp');
const lifecycle = read('MCP/Transport/McpNativeTransportLifecycle.cpp');
const gateway = read('MCP/Transport/McpNativeTransportGateway.cpp');
const managerHeader = read('MCP/DynamicTools/McpDynamicToolManager.h');
const managerDispatch = read('MCP/DynamicTools/McpDynamicToolManager.cpp');
const toolToggles = read('MCP/DynamicTools/McpDynamicToolManagerToolToggles.cpp');
const categoryToggles = read('MCP/DynamicTools/McpDynamicToolManagerCategoryToggles.cpp');
const queries = read('MCP/DynamicTools/McpDynamicToolManagerQueries.cpp');
const capabilityStore = read('MCP/Gateway/McpNativeGatewayCapabilityStore.cpp');
const receipt = read('MCP/Execute/McpNativeGatewayReceipt.cpp');
const search = read('MCP/Gateway/McpNativeGatewaySearch.cpp');
const describeOp = read('MCP/Gateway/McpNativeGatewayDescribe.cpp');

/** Runtime visibility counter Task 28 introduces, distinct from the generated fingerprint. */
const STATE_REVISION = 'CatalogStateRevision';

const between = (source: string, startMarker: string, endMarker: string): string =>
  source.slice(source.indexOf(startMarker), source.indexOf(endMarker));

/** The `capabilities` object built by HandleInitialize. */
const capabilitiesBlock = between(
  discovery,
  'auto Capabilities = MakeShared<FJsonObject>();',
  'auto ServerInfo = MakeShared<FJsonObject>();',
);

const onToolsListChanged = between(
  sessions,
  'void FMcpNativeTransport::OnToolsListChanged()',
  'void FMcpNativeTransport::BroadcastToolsListChanged()',
);

const initializeBody = between(
  managerDispatch,
  'void FMcpDynamicToolManager::Initialize(',
  'bool FMcpDynamicToolManager::IsToolEnabled_NoLock(',
);

/** Each dispatch site that fires the change delegate behind an effective `bChanged`. */
const effectiveMutationSites = (): string[] =>
  managerDispatch.match(
    /if \(bChanged\)[\s\S]{0,240}?OnToolsChanged\.ExecuteIfBound\(\);/g,
  ) ?? [];

describe('Task 28 BASELINE: native capability + catalog-change behavior to preserve', () => {
  it('routes the dynamic manager change delegate into the transport notifier', () => {
    expect(lifecycle).toContain(
      'ToolManager.OnToolsChanged.BindRaw(this, &FMcpNativeTransport::OnToolsListChanged);',
    );
    expect(managerHeader).toContain('DECLARE_DELEGATE(FOnToolsChanged);');
    expect(managerHeader).toContain('FOnToolsChanged OnToolsChanged;');
  });

  it('permanently suppresses the list-changed broadcast', () => {
    // Task 30 cutover: the public surface is a permanent single static tool, so
    // suppression is unconditional — no gateway-mode gate and no reachable broadcast.
    expect(onToolsListChanged).not.toContain('if (bGatewayMode)');
    expect(onToolsListChanged).not.toContain('BroadcastToolsListChanged();');
    // Suppression is explicit, not an accidental empty body: the handler logs it.
    expect(onToolsListChanged).toContain(
      'suppressed (public surface is a static single tool)',
    );
  });

  it('keeps the legacy non-gateway notifications/tools/list_changed broadcast', () => {
    // Task 30 owns removing the legacy public listing; until then this stays.
    expect(sessions).toContain('void FMcpNativeTransport::BroadcastToolsListChanged()');
    expect(sessions).toContain('TEXT("notifications/tools/list_changed")');
    expect(sessions).toContain('BroadcastNotification(');
  });

  it('reports an effective bChanged out-param from every mutation method', () => {
    expect(managerHeader).toContain('EnableTools(const TArray<FString>& ToolNames, bool& bOutChanged)');
    expect(managerHeader).toContain('DisableTools(const TArray<FString>& ToolNames, bool& bOutChanged)');
    expect(managerHeader).toContain('EnableCategory(const FString& Category, bool& bOutChanged)');
    expect(managerHeader).toContain('DisableCategory(const FString& Category, bool& bOutChanged)');
    expect(managerHeader).toContain('Reset(bool& bOutChanged)');
    // Effectiveness, not mere invocation: each impl derives the flag from real state deltas.
    expect(toolToggles).toContain('bOutChanged = bAnyActualChange;');
    expect(categoryToggles).toContain('bOutChanged = (Enabled.Num() > 0) || bAnyCategoryToggled;');
    expect(categoryToggles).toContain('bOutChanged = (Disabled.Num() > 0) || bAnyCategoryToggled;');
    expect(queries).toContain('bOutChanged = (Changed > 0);');
    expect(effectiveMutationSites()).toHaveLength(5);
  });

  it('keeps protected-tool, protected-category and no-op mutations guarded', () => {
    expect(managerDispatch).toContain('return Name == TEXT("manage_tools") || Name == TEXT("inspect");');
    expect(managerDispatch).toContain('return Name == TEXT("core");');
    expect(toolToggles).toContain('if (IsProtectedTool(Name))');
    expect(categoryToggles).toContain('is protected and cannot be disabled');
    // A rejected protected-category disable returns before touching bOutChanged.
    const disableCategory = categoryToggles.slice(
      categoryToggles.indexOf('FMcpDynamicToolManager::DisableCategory('),
    );
    expect(disableCategory.indexOf('IsProtectedCategory(Category)')).toBeLessThan(
      disableCategory.indexOf('bOutChanged ='),
    );
  });

  it('advertises the Task 37 wired primitives on native initialize (tools + resources.subscribe + prompts + completions), never logging/tasks', () => {
    // Task 37 wired the native resources/prompts/completions primitives through
    // HandlePrimitiveMethod, so the native initialize surface now advertises them
    // truthfully. Only genuinely-unbacked primitives stay off the surface.
    expect(capabilitiesBlock).toContain('SetObjectField(TEXT("tools")');
    expect(capabilitiesBlock).toContain('TEXT("resources")');
    expect(capabilitiesBlock).toContain('TEXT("subscribe")');
    expect(capabilitiesBlock).toContain('TEXT("prompts")');
    expect(capabilitiesBlock).toContain('TEXT("completions")');
    // Task 44 backed the native tasks/* surface (FMcpTaskSurface), so tasks is
    // advertised here too — and only here, alongside the requests.tools.call
    // claim that a tools/call may be task-augmented.
    expect(capabilitiesBlock).toContain('TEXT("tasks")');
    expect(capabilitiesBlock).toContain('TEXT("cancel")');
    for (const unbacked of ['TEXT("logging")', 'TEXT("listChanged")']) {
      expect(capabilitiesBlock).not.toContain(unbacked);
    }
  });

  it('keeps the generated catalogRevision an immutable content fingerprint', () => {
    expect(capabilityStore).toContain(
      'Store.CatalogRevision = McpGeneratedCapabilityShards::CatalogRevision();',
    );
    expect(receipt).toContain('TEXT("catalogRevision"), FMcpCanonicalRecordIndex::Get().GetCatalogRevision())');
    expect(search).toContain('Out->SetStringField(TEXT("catalogRevision"), Revision);');
  });

  it('routes gateway configure through the dynamic manager without its own broadcast', () => {
    expect(gateway).toContain('if (Operation == TEXT("configure"))');
    expect(gateway).toContain('ToolManager.HandleAction(Action, ManageArgs)');
    // Gateway mutations must not emit a tools/list_changed of their own.
    expect(gateway).not.toContain('BroadcastToolsListChanged');
    expect(gateway).not.toContain('notifications/tools/list_changed');
  });
});

describe('Task 28 DESIRED: truthful capability advertisement + catalog state revision', () => {
  it('advertises tools always and permanently omits tools.listChanged', () => {
    expect(capabilitiesBlock).toContain('SetObjectField(TEXT("tools")');
    // Task 30 cutover: the single static tool never changes shape, so listChanged
    // is permanently omitted — never advertised false, never true, no gateway gate.
    expect(discovery).not.toContain('SetBoolField(TEXT("listChanged"), false)');
    expect(discovery).not.toContain('ToolsCap->SetBoolField(TEXT("listChanged"), true);');
    expect(capabilitiesBlock).not.toContain('bGatewayMode');
    expect(discovery).not.toContain('SetBoolField(TEXT("listChanged")');
  });

  it('gives the dynamic manager a monotonic catalogStateRevision with a typed getter', () => {
    expect(managerHeader).toContain(STATE_REVISION);
    expect(managerHeader).toMatch(
      new RegExp(`\\b(?:uint64|int64|uint32|int32)\\s+Get\\w*${STATE_REVISION}\\w*\\s*\\(\\s*\\)\\s*const`),
    );
    // Deterministic starting value, established with the rest of the initial state.
    expect(initializeBody).toContain(STATE_REVISION);
    // Distinct from the immutable generated fingerprint; never reuses it.
    expect(managerHeader).not.toContain('McpGeneratedCapabilityShards');
    expect(managerDispatch).not.toContain('McpGeneratedCapabilityShards::CatalogRevision');
  });

  it('increments the state revision exactly once per effective mutation batch', () => {
    const sites = effectiveMutationSites();
    expect(sites).toHaveLength(5);
    for (const site of sites) {
      expect(site).toContain(STATE_REVISION);
      // Observers woken by OnToolsChanged must already see the new revision.
      expect(site.indexOf(STATE_REVISION)).toBeLessThan(
        site.indexOf('OnToolsChanged.ExecuteIfBound'),
      );
    }
    // No-op and rejected mutations return through the toggle impls, which never bump.
    expect(toolToggles).not.toContain(STATE_REVISION);
    expect(categoryToggles).not.toContain(STATE_REVISION);
  });

  it('keeps state-revision access thread-safe and ordered with existing state locking', () => {
    expect(managerHeader).toContain(STATE_REVISION);
    // Either an atomic counter or the existing StateMutex may own it; both are safe.
    const atomicallyOwned = /std::atomic|TAtomic<|FThreadSafeCounter/.test(managerHeader);
    const mutexOwned = new RegExp(
      `(StateMutex[\\s\\S]{0,400}${STATE_REVISION}|${STATE_REVISION}[\\s\\S]{0,400}StateMutex)`,
    ).test(`${managerHeader}\n${managerDispatch}\n${queries}`);
    expect(atomicallyOwned || mutexOwned).toBe(true);
    expect(managerHeader).toContain('mutable FCriticalSection StateMutex;');
  });

  it('exposes both revisions on configure/status only, never on execute receipts', () => {
    expect(queries).toContain('catalogStateRevision');
    expect(queries).toContain('catalogRevision');
    const getStatus = queries.slice(queries.indexOf('FMcpDynamicToolManager::GetStatus('));
    expect(getStatus).toContain('catalogStateRevision');
    // The mutable counter stays out of search/describe/execute payloads (Tasks 39/42 own receipts).
    expect(receipt).not.toContain('catalogStateRevision');
    expect(search).not.toContain('catalogStateRevision');
    expect(describeOp).not.toContain('catalogStateRevision');
  });
});
