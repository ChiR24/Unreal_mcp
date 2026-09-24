/// <reference types="node" />

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const privateSource = (...parts: string[]): string =>
  readFileSync(
    resolve(
      process.cwd(),
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private',
      ...parts,
    ),
    'utf8',
  );

describe('plugin runtime behavior contracts', () => {
  it('applies byte and enum JSON settings before rejecting unsupported properties', () => {
    const source = privateSource(
      'Foundation',
      'Reflection',
      'McpPropertyReflectionImport.cpp',
    );

    const byteHandler = source.indexOf(
      'if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))',
    );
    const enumHandler = source.indexOf(
      'if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))',
    );
    const unsupportedFallback = source.indexOf(
      'Unsupported property type: %s',
    );

    expect(byteHandler).toBeGreaterThan(-1);
    expect(enumHandler).toBeGreaterThan(byteHandler);
    expect(unsupportedFallback).toBeGreaterThan(enumHandler);
  });

  it('validates enum inputs before conversion and rejects hidden sentinels', () => {
    const source = privateSource(
      'Foundation',
      'Reflection',
      'McpPropertyReflectionImport.cpp',
    );

    expect(source).toContain("Name[Index] == TEXT('\\0')");
    expect(source).toContain('if (bContainsEmbeddedNull)');
    expect(source).toContain('FMath::IsFinite(Value)');
    expect(source).toContain('FMath::Frac(Value) != 0.0');
    expect(source).toContain(
      'Value < static_cast<double>(MIN_int64)',
    );
    expect(source).toContain('Value >= Int64UpperBound');
    expect(source).toContain('IntValue < MIN_int32');
    expect(source).toContain('IntValue > MAX_int32');
    expect(source).toContain('Enum->GetIndexByValue(OutValue)');
    expect(source).toContain('Enum->HasMetaData(TEXT("Hidden"), EnumIndex)');
    expect(source).toContain('Enum->NumEnums() - 1');
    expect(source).not.toContain('GenerateFullEnumName');
    expect(source).not.toContain(
      'OutValue == INDEX_NONE || !Enum->IsValidEnumValue(OutValue)',
    );
  });

  it('distinguishes omitted channel textures from failed channel texture loads', () => {
    const source = privateSource(
      'Domains',
      'Texture',
      'McpAutomationBridge_TextureHandlersChannelPack.cpp',
    );

    expect(source).not.toContain('Response->GetBoolField(TEXT("success"))');
    for (const channel of ['Red', 'Green', 'Blue', 'Alpha']) {
      expect(source).toContain(
        `if (!${channel}Path.IsEmpty() && !${channel}Tex)`,
      );
    }
  });

  it('resolves Blueprint root aliases even when no explicit root node exists', () => {
    const supportSource = privateSource(
      'Domains',
      'SCS',
      'McpAutomationBridge_SCSHandlers.cpp',
    );
    const addSource = privateSource(
      'Domains',
      'SCS',
      'McpAutomationBridge_SCSHandlersAddComponent.cpp',
    );

    expect(supportSource).toContain('bool IsSCSRootAlias(');
    expect(supportSource).toMatch(
      /IsSCSRootAlias\(ExpectedParentName\)[\s\S]*IsSCSRootNode\(SCS, Node\)/u,
    );
    expect(addSource).toContain('IsSCSRootAlias(ParentComponentName)');
    expect(addSource).toMatch(
      /IsSCSRootAlias\(ParentComponentName\)[\s\S]*SCS->GetRootNodes\(\)/u,
    );
  });

  it('adds Niagara modules through the stack graph utility', () => {
    const source = privateSource(
      'Domains',
      'NiagaraGraph',
      'McpAutomationBridge_NiagaraGraphHandlers.cpp',
    );

    expect(source).toContain(
      'FNiagaraStackGraphUtilities::AddScriptModuleToStack',
    );
    expect(source).not.toContain(
      'NewObject<UNiagaraNodeFunctionCall>(TargetGraph)',
    );
  });

  it('splices the Niagara parameter-map chain before removing stack modules', () => {
    const handlerSource = privateSource(
      'Domains',
      'NiagaraGraph',
      'McpAutomationBridge_NiagaraGraphHandlers.cpp',
    );
    const removalSource = privateSource(
      'Domains',
      'NiagaraGraph',
      'McpAutomationBridge_NiagaraGraphRemoval.cpp',
    );

    expect(handlerSource).toContain('RemoveNiagaraGraphNodeSafely(');
    expect(handlerSource).not.toContain(
      'TargetGraph->RemoveNode(TargetNode)',
    );
    expect(removalSource).toMatch(
      /TryCreateConnection\(\s*PreviousOutputPin,\s*NextInputPin\)[\s\S]*RemoveNode\(TargetNode\)/u,
    );
  });

  it('prefers an existing Niagara connection before attempting auto-connect', () => {
    const source = privateSource(
      'Domains',
      'NiagaraGraph',
      'McpAutomationBridge_NiagaraGraphPins.cpp',
    );
    const existingConnectionCheck = source.indexOf(
      'if (!FromCandidatePin->LinkedTo.IsEmpty())',
    );
    const connectionAttempt = source.indexOf(
      'TryCreateConnection(FromCandidatePin, ToCandidatePin)',
    );

    expect(existingConnectionCheck).toBeGreaterThan(-1);
    expect(connectionAttempt).toBeGreaterThan(-1);
    expect(existingConnectionCheck).toBeLessThan(connectionAttempt);
  });

  it('switches to a transient editor world before deleting active worlds', () => {
    const source = privateSource(
      'Safety',
      'McpSafeOperationsFolderDeleteAssets.h',
    );

    expect(source).toContain('GEditor->NewMap(false)');
    expect(source).not.toContain('TEXT("/Engine/Maps/');
  });

  it('verifies that map loading changed to the requested world package', () => {
    const source = privateSource('Safety', 'McpSafeOperationsMapLoad.h');

    expect(source).toContain('ResolveExpectedMapPackageName(');
    expect(source).toMatch(
      /FEditorFileUtils::LoadMap\(\*MapPath\)[\s\S]*LoadedWorldPackageName\.Equals\(\s*ExpectedPackageName/u,
    );
    expect(source).toContain(
      'McpSafeLoadMap: Editor world does not match requested map',
    );
  });

  it('emits subscribed logs as schema-valid structured automation events', () => {
    const source = privateSource(
      'Domains',
      'Log',
      'McpAutomationBridge_LogHandlers.cpp',
    );
    const responseSource = privateSource(
      'Core',
      'Subsystem',
      'McpAutomationBridgeSubsystemResponses.cpp',
    );
    const transportHeader = privateSource(
      'MCP',
      'Transport',
      'McpNativeTransport.h',
    );
    const transportNotifications = privateSource(
      'MCP',
      'Transport',
      'McpNativeTransportNotifications.cpp',
    );

    expect(source).toContain(
      'Event->SetStringField(TEXT("type"), TEXT("automation_event"))',
    );
    expect(source).toContain(
      'Event->SetObjectField(TEXT("payload"), Payload)',
    );
    expect(source).toContain('StrongSubsystem->BroadcastAutomationEvent(Event)');
    expect(source).not.toContain('StrongSubsystem->SendRawMessage');
    expect(source).toContain('SendAutomationResponse(');
    expect(source).not.toContain('SendRawMessage(Serialized)');
    expect(responseSource).toContain(
      'UMcpAutomationBridgeSubsystem::BroadcastAutomationEvent',
    );
    expect(responseSource).toContain('SendRawMessageToLogSubscribers');
    expect(responseSource).toContain('SendRawMessageToSocket');
    expect(responseSource).toContain(
      'BroadcastLogEventNotification(Event)',
    );
    expect(transportHeader).toContain('BroadcastNotification(');
    expect(transportHeader).toContain(
      'BroadcastLogEventNotification(',
    );
    expect(transportNotifications).toContain(
      'FMcpNativeTransport::BroadcastNotification',
    );
    expect(transportNotifications).toContain('IsLogAutomationEvent');
    expect(transportNotifications).toContain('QueueNotificationEventWrites');
    expect(transportNotifications).toContain(
      'SubscribedSessions.Contains(Stream->SessionId)',
    );
    expect(transportNotifications).toContain(
      'TEXT("notifications/unreal/automation_event")',
    );
    expect(source).toContain(
      'SetLogEventSubscriptionForRequest',
    );
    expect(source).toContain('ConnectionManager->SetLogSubscription');
    expect(source).toContain('HasLogEventSubscribers');
    expect(source).toContain('HasLogSubscribers');
    expect(source).not.toContain(
      'TEXT("{\\"event\\":\\"log\\",\\"category\\"',
    );
  });

  // UEditorAssetLibrary and GetAllLevelActors refuse every call during PIE and
  // only log it, so ~190 handler lookups answered "not found" for things that
  // exist. The one response funnel names the real cause on the failure.
  it('names Play In Editor as the cause when an editor-only lookup was refused', () => {
    const responseSource = privateSource('Core', 'Subsystem', 'McpAutomationBridgeSubsystemResponses.cpp');
    const enrichment = privateSource('Core', 'Subsystem', 'McpAutomationBridgeSubsystemResponseEnrichment.h');

    expect(responseSource).toMatch(
      /if \(!bEffectiveSuccess\)\s*\{\s*EffectiveMessage = SanitizeEngineErrorForResponse\(EffectiveMessage\);\s*FScopeLock Lock\(&ErrorCaptureMutex\);\s*McpAppendPieRefusalHint\(EffectiveMessage, bProcessingAutomationRequest && GEditor && GEditor->PlayWorld, CurrentErrorCapture\.ErrorMessages\);/,
    );
    expect(enrichment).toContain('Error.Contains(TEXT("currently in a play mode"))');
    expect(enrichment).toContain('Stop play (control_editor, action');
  });

  it('routes blueprint completion automation events through the subsystem broadcaster', () => {
    const eventSources = [
      privateSource('Domains', 'Blueprint', 'Graph', 'McpAutomationBridge_BlueprintHandlersAddNodeResponse.cpp'),
      privateSource('Domains', 'Blueprint', 'Components', 'McpAutomationBridge_BlueprintHandlersModifyScsFinalize.cpp'),
      privateSource('Domains', 'Blueprint', 'Functions', 'McpAutomationBridge_BlueprintHandlersAddFunctionResult.cpp'),
      privateSource('Domains', 'Blueprint', 'Variables', 'McpAutomationBridge_BlueprintHandlersVariableMetadata.cpp'),
      privateSource('Domains', 'Blueprint', 'Events', 'McpAutomationBridge_BlueprintHandlersAddEventResult.cpp'),
      privateSource('Domains', 'Blueprint', 'Events', 'McpAutomationBridge_BlueprintHandlersRemoveEvent.cpp'),
    ];

    for (const source of eventSources) {
      expect(source).toContain(
        'SetStringField(TEXT("type"), TEXT("automation_event"))',
      );
      expect(source).toContain('Bridge.BroadcastAutomationEvent(Notify, RequestingSocket)');
      expect(source).not.toContain('SendControlMessage(Notify)');
    }
  });

  it('locks Unreal Insights tracing to loopback and profiling trace files', () => {
    const validation = privateSource(
      'Domains',
      'Insights',
      'McpAutomationBridge_InsightsValidation.cpp',
    );
    const traceControl = privateSource(
      'Domains',
      'Insights',
      'McpAutomationBridge_InsightsTraceControl.cpp',
    );
    const traceAnalysis = privateSource(
      'Domains',
      'Insights',
      'McpAutomationBridge_InsightsTraceAnalysis.cpp',
    );
    const traceState = privateSource(
      'Domains',
      'Insights',
      'McpAutomationBridge_InsightsTraceState.cpp',
    );

    expect(validation).toContain('IsLoopbackHost');
    expect(validation).toContain('NON_LOOPBACK_HOST_DENIED');
    expect(validation).toContain('FPaths::ProfilingDir()');
    expect(validation).toContain(
      'Trace file path must stay under the project Saved/Profiling directory.',
    );
    expect(validation).toMatch(
      /bRequireExistingFile[\s\S]*GetExtension\(Path\)\.Equals\(TEXT\("utrace"\)/u,
    );
    expect(traceControl).toContain('Mode == TEXT("relay")');
    expect(traceControl).toContain('Mode == TEXT("none")');
    expect(traceControl).toContain('Mode == TEXT("memory")');
    expect(traceControl).not.toContain(
      'FTraceAuxiliary::EConnectionType::None',
    );
    expect(traceState).toMatch(
      /if \(!HasActiveTraceForStateChange\(\)\)[\s\S]*TRACE_PAUSE_FAILED/u,
    );
    expect(traceState).toMatch(
      /if \(!HasActiveTraceForStateChange\(\)\)[\s\S]*TRACE_RESUME_FAILED/u,
    );
    expect(traceAnalysis).toContain(
      'TryResolveTracePath(Payload, true, false, false',
    );
  });

  it('bounds custom handler alias config before parsing', () => {
    const source = privateSource(
      'Core',
      'Subsystem',
      'McpAutomationBridgeSubsystemCustomHandlerAliasConfig.cpp',
    );

    const sizeCheck = source.indexOf('MaxAliasConfigBytes');
    const loadFile = source.indexOf('FFileHelper::LoadFileToString');
    expect(sizeCheck).toBeGreaterThan(-1);
    expect(loadFile).toBeGreaterThan(sizeCheck);
    expect(source).toContain('FileSize > MaxAliasConfigBytes');
    expect(source).toContain('unsupported size');
    expect(source).toContain('TryGetNumberField(TEXT("version"), Version)');
    expect(source).toContain('FMath::IsNearlyEqual(Version, 1.0)');
    expect(source).not.toContain('TryGetArrayField(TEXT("handlerAliases")');
    expect(source).not.toContain('TryGetStringField(TEXT("action")');
    expect(source).not.toContain('TryGetStringField(TEXT("targetAction")');
  });

  it('does not attach a transient nested emitter to a new Niagara system', () => {
    const source = privateSource(
      'Domains',
      'NiagaraAuthoring',
      'McpAutomationBridge_NiagaraAuthoringHandlersSystems.cpp',
    );

    expect(source).not.toContain(
      'NewObject<UNiagaraEmitter>(NewSystem, FName(TEXT("DefaultEmitter")))',
    );
  });

  it('makes WebSocket listener shutdown idempotent and race-safe', () => {
    const header = privateSource(
      'Transport',
      'WebSocket',
      'McpBridgeWebSocket.h',
    );
    const lifecycleSource = privateSource(
      'Transport',
      'WebSocket',
      'McpBridgeWebSocket.cpp',
    );
    const serverSource = privateSource(
      'Transport',
      'WebSocket',
      'McpBridgeWebSocketServer.cpp',
    );

    expect(header).toContain('TAtomic<bool> bCloseStarted;');
    expect(header).toContain('FCriticalSection ListenSocketMutex;');
    expect(lifecycleSource).toContain('if (bCloseStarted.Exchange(true))');
    expect(lifecycleSource).toContain('CloseListenSocket();');
    expect(serverSource).toContain('ON_SCOPE_EXIT');
    expect(serverSource).toContain('DestroyListenSocket();');
    expect(serverSource).not.toMatch(
      /DestroySocket\(ListenSocket\)[\s\S]{0,80}ListenSocket = nullptr/u,
    );
  });
});
