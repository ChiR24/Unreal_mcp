import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

const plugin = 'plugins/EngineWire/Source/EngineWire';
const source = (file: string) => readFileSync(resolve(plugin, file), 'utf8');
const data = (file: string) => source(`Private/Domains/PCG/Data/${file}`);

describe('PCG data-view integration source boundaries (not native behavior)', () => {
  it('owns bounded view state on the existing subsystem and purges on its existing safe tick', () => {
    const header = source('Public/EngineWireSubsystem.h');
    const lifecycle = source('Private/Core/Subsystem/EngineWireSubsystemLifecycle.cpp');
    expect(header).toContain('TSharedPtr<McpPCGHandlers::FPCGDataViewStore> PCGDataViews');
    expect(lifecycle).toContain('PCGDataViews.Reset();');
    expect(lifecycle).toContain('if (PCGDataViews.IsValid()) PCGDataViews->Purge();');
    for (const file of ['McpPCGDataViews.cpp', 'McpPCGDataActions.cpp', 'McpPCGDataQuery.cpp', 'McpPCGDataCapture.cpp']) {
      expect(data(file)).not.toMatch(/AddTicker|TMap<.*>\s+Global|AddToRoot|GetOrComputeCrc|GetPointsCopy|ToPointData/);
    }
  });

  it('reaches data actions through the existing parent route without a second queue', () => {
    const handlers = source('Private/Domains/PCG/McpPCGHandlers.cpp');
    expect(handlers).toContain('McpPCGHandlers::IsPCGDataAction(SubAction)');
    expect(handlers).toContain('McpPCGHandlers::HandlePCGData(this, RequestId, SubAction, Payload, Socket, *PCGDataViews)');
    expect(handlers).toContain('PCG_DATA_NOT_AVAILABLE');
    expect(handlers.indexOf('IsPCGDataAction')).toBeLessThan(handlers.indexOf('IsTransactionActive'));
  });

  it('keeps exact trusted owner/target and bounded complete result checks native', () => {
    const actions = data('McpPCGDataActions.cpp');
    expect(actions).toContain('ResolveExactPCGComponent(Payload, Error)');
    expect(actions).toContain('ResolvePCGPrincipal(Bridge, RequestId, Socket, Principal)');
    expect(actions).toContain('EWorldType::Editor');
    expect(actions).toContain('Component->IsPartitioned()');
    expect(actions).toContain('Component->IsManagedByRuntimeGenSystem()');
    expect(actions).toContain('HasActivePCGOperation(Component)');
    expect(actions).toContain('Encoded.Len() > 80000');
    expect(actions).toContain('!Budget.IsWithinTime()');
    expect(actions).toContain('if (!bPublished) for (const FString& Id : NewHandles) Store.Release');
    expect(actions).not.toMatch(/GenerateLocal|EnableInspection|McpSafe.*Save|QueueAutomationRequest/);
  });

  it('retains immutable projected rows and metadata rather than caller-owned mutable JSON', () => {
    const views = data('McpPCGDataViews.cpp');
    expect(views).toContain('Snapshot->GetCharacterCount() + EncodedDescription.Len()');
    expect(views).toContain('TJsonReaderFactory<TCHAR>::Create(Entry->EncodedDescription)');
    expect(views).toContain('Entry->Owner != Principal');
    expect(views).toContain('Entry->Component.Get() != Component');
    const snapshot = data('McpPCGDataSnapshot.cpp');
    expect(snapshot).toContain('const auto Invalidated = Snapshot->Invalidated');
    expect(snapshot).toContain('[Invalidated](UPCGComponent*) { Invalidated->store(true); }');
    expect(snapshot).not.toContain('Weak.Pin()');
    expect(data('McpPCGDataSnapshot.h')).toContain('TSharedRef<std::atomic<bool>, ESPMode::ThreadSafe>');
  });

  it('bounds selected native construction without implicit point conversion or operation lookup', () => {
    const rows = data('McpPCGDataRows.cpp');
    expect(rows).toContain('Projection.Limit > FPCGDataSnapshot::MaxRows');
    expect(rows).toContain('ValidatePCGPointFields(Projection.Fields, OutError)');
    expect(rows).toContain('GetConstMetadataEntryValueRange()');
    expect(rows).toContain('FPCGDataSnapshot::MaxRowCharacters');
    expect(rows).toContain('FPCGDataSnapshot::MaxCharacters');
    const capture = data('McpPCGDataCapture.cpp');
    expect(capture).not.toMatch(/FindPCGOperation|GetOrComputeCrc|ToPointData|GetPointsCopy|MutableMetadata/);
  });
});
