import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

import {
  privateSource,
  publicSource,
} from './sequence_contract_test_utils.js';

const nativeSchemaScenario = (): string => readFileSync(
  resolve(process.cwd(), 'scripts', 'sequence-native-direct-schema.mjs'),
  'utf8',
);

describe('sequence render and native security contracts', () => {
  it('cancels timed-out MRQ work without same-stack PIE teardown', () => {
    const completion = privateSource(
      'Domains',
      'Sequence',
      'MovieRender',
      'McpAutomationBridge_SequenceMovieRenderCompletion.cpp',
    );
    const execution = privateSource(
      'Domains',
      'Sequence',
      'MovieRender',
      'McpAutomationBridge_SequenceMovieRenderExecution.cpp',
    );
    const response = privateSource(
      'Domains',
      'Sequence',
      'MovieRender',
      'McpAutomationBridge_SequenceMovieRenderResponse.cpp',
    );

    expect(completion).toContain('RequestEndPlayMap()');
    expect(completion).toContain('State->bTimedOut = true');
    expect(completion).toContain('State->CancellationHandle');
    expect(completion).not.toContain(
      'if (bTimedOut && Executor && Executor->IsRendering())\n' +
        '    Executor->CancelAllJobs();',
    );
    expect(completion).toContain('State->bCancellationRequested');
    expect(response).toMatch(
      /TEXT\("renderContinuesAsynchronously"\),\s+bRenderStillActive/,
    );
    expect(completion).toContain('CancellationDeadlineSeconds');
    expect(completion).toContain('FPlatformTime::Seconds()');
    expect(completion).toContain('MaxMovieRenderCancellationWaitMs');
    expect(response).toMatch(
      /State->bTimedOut && Executor && Executor->IsRendering\(\) &&\s+!State->bCancellationDeadlineExpired/,
    );
    expect(response).not.toContain('render continues asynchronously');
    expect(execution).toContain('ValidateQueueResourceLimits');
  });

  it('retains MRQ ownership after client deletion until cancellation settles', () => {
    const completion = privateSource(
      'Domains',
      'Sequence',
      'MovieRender',
      'McpAutomationBridge_SequenceMovieRenderCompletion.cpp',
    );
    const cancelStart = completion.slice(
      completion.indexOf('void CancelStartRender('),
      completion.indexOf('void BeginTimedOutRenderCancellation('),
    );
    const response = privateSource(
      'Domains',
      'Sequence',
      'MovieRender',
      'McpAutomationBridge_SequenceMovieRenderResponse.cpp',
    );

    expect(cancelStart).toContain('State->bClientDisconnected = true');
    expect(cancelStart).toContain('State->CancellationHandle');
    expect(cancelStart).toContain('CurrentExecutor->IsRendering()');
    expect(cancelStart.indexOf('TryDispatchRenderCancellation')).toBeLessThan(
      cancelStart.lastIndexOf('DiscardPreparedRenderStart'),
    );
    expect(cancelStart).not.toContain(
      'State->bCompleted = true;\n' +
        '  State->bCancellationRequested = true;',
    );
    expect(response).toContain(
      'if (State->bClientDisconnected)',
    );
  });

  it('monitors the MRQ output path while the executor is active', () => {
    const execution = privateSource(
      'Domains',
      'Sequence',
      'MovieRender',
      'McpAutomationBridge_SequenceMovieRenderExecution.cpp',
    );
    const response = privateSource(
      'Domains',
      'Sequence',
      'MovieRender',
      'McpAutomationBridge_SequenceMovieRenderResponse.cpp',
    );

    expect(execution).toContain('State->OutputPathCheckHandle');
    expect(execution).toContain('ValidateRenderOutputDirectory(');
    expect(execution).toContain('State->bOutputPathInvalidated = true');
    expect(execution).toContain('RequestRenderCancellation');
    expect(response).toContain('MRQ_OUTPUT_PATH_CHANGED');
    expect(response).toContain('path_revalidation_failed');
  });

  it('enforces plugin-side MRQ resource ceilings', () => {
    const settings = publicSource('McpAutomationBridgeSettings.h');
    const limits = privateSource(
      'Domains',
      'Sequence',
      'MovieRender',
      'McpAutomationBridge_SequenceMovieRenderResourceLimits.cpp',
    );
    const queueLimits = privateSource(
      'Domains',
      'Sequence',
      'MovieRender',
      'McpAutomationBridge_SequenceMovieRenderQueueLimits.cpp',
    );
    const renderSettings = privateSource(
      'Domains',
      'Sequence',
      'MovieRender',
      'McpAutomationBridge_SequenceMovieRenderSettings.cpp',
    );
    const execution = privateSource(
      'Domains',
      'Sequence',
      'MovieRender',
      'McpAutomationBridge_SequenceMovieRenderExecution.cpp',
    );

    for (const name of [
      'MaxMovieRenderResolutionDimension',
      'MaxMovieRenderPixelCount',
      'MaxMovieRenderFrameCount',
      'MaxMovieRenderSampleCount',
      'MaxMovieRenderCombinedSampleCount',
      'MaxMovieRenderConsoleVariables',
      'MaxMovieRenderTimeoutMs',
      'MaxMovieRenderCancellationWaitMs',
      'MaxMovieRenderQueueJobs',
      'MaxMovieRenderEnabledJobs',
      'MaxMovieRenderAggregateWork',
      'MaxMovieRenderOutputScanFiles',
      'MovieRenderBurnInClassAllowlist',
      'MaxTakeRecorderSourceItems',
      'MaxTakeRecorderStringLength',
    ]) {
      expect(settings).toContain(name);
    }
    expect(limits).toContain('MRQ_RESOURCE_LIMIT_EXCEEDED');
    expect(limits).toContain('MovieRenderConsoleVariableAllowlist');
    expect(queueLimits).toContain('MovieScene->GetPlaybackRange()');
    expect(renderSettings).toContain('ValidateSampleResourceLimits');
    expect(renderSettings).toContain(
      'ValidateConsoleVariableResourceLimits',
    );
    expect(execution).toContain('ValidateRenderTimeoutResourceLimit');
    expect(execution).toContain('DiscardPreparedRenderStart');
    expect(execution.lastIndexOf('ValidateJobForExecution(')).toBeLessThan(
      execution.indexOf('RenderQueueWithExecutorInstance(Executor)'),
    );
  });

  it('validates custom MRQ playback ranges against the per-job frame limit', () => {
    const limits = privateSource(
      'Domains',
      'Sequence',
      'MovieRender',
      'McpAutomationBridge_SequenceMovieRenderResourceLimits.cpp',
    );

    // The function body must actually consult bHasCustomRange and the range
    // endpoints now (previously it discarded them).
    expect(limits).toContain('bHasCustomRange');
    expect(limits).toContain('INVALID_FRAME_RANGE');
    expect(limits).toMatch(
      /if\s*\(\s*bHasCustomRange\s*\)\s*\{[\s\S]*?MaxMovieRenderFrameCount/,
    );
    // Per-shot handle count is still enforced regardless of the custom range flag.
    expect(limits).toContain('MaxMovieRenderHandleFrameCount');
    // The downstream caller uses the same range-check contract.
    expect(limits).toContain('EffectiveFrames');
  });

  it('enforces queue resource limits before and after job allocation', () => {
    const creation = privateSource(
      'Domains',
      'Sequence',
      'MovieRender',
      'McpAutomationBridge_SequenceMovieRenderJobCreation.cpp',
    );
    const preflight = creation.indexOf('ValidateQueueResourceLimits(');
    const allocation = creation.indexOf('CreateJobFromSequence(');
    const postflight = creation.lastIndexOf('ValidateQueueResourceLimits(');

    expect(preflight).toBeGreaterThan(-1);
    expect(preflight).toBeLessThan(allocation);
    expect(postflight).toBeGreaterThan(allocation);
    expect(creation).toContain('RollBackJob();');
  });

  it('exercises queue overflow rollback in the native live scenario', () => {
    const scenario = [
      'sequence-native-direct-queue.mjs',
      'sequence-native-direct-queue-limit.mjs',
      'sequence-native-direct-queue-python.mjs',
      'sequence-native-direct-queue-restore.mjs',
    ].map((fileName) =>
      readFileSync(
        resolve(process.cwd(), 'scripts', fileName),
        'utf8',
      ),
    ).join('\n');

    expect(scenario).toContain('verifyRenderQueueCreationLimit');
    expect(scenario).toContain(
      'reject native MRQ job beyond configured queue limit',
    );
    expect(scenario).toContain(
      'verify rejected native MRQ job was rolled back',
    );
    expect(scenario).toContain('remainingLimitJobs');
  });

  it('revalidates a configured MRQ output path immediately before start', () => {
    const nativeScenario = readFileSync(
      resolve(
        process.cwd(),
        'scripts',
        'sequence-native-direct-render-job.mjs',
      ),
      'utf8',
    );

    expect(nativeScenario).toContain(
      'reject swapped native render output symlink',
    );
    expect(nativeScenario).toContain('MRQ_OUTPUT_PATH_NOT_ALLOWED');
    expect(nativeScenario).toContain(
      "await fs.symlink('/etc', context.outputDirectory, 'dir')",
    );
  });

  it('restricts MRQ executors and Take Recorder source classes', () => {
    const executor = privateSource(
      'Domains',
      'Sequence',
      'MovieRender',
      'McpAutomationBridge_SequenceMovieRenderExecutor.cpp',
    );
    const takeSources = privateSource(
      'Domains',
      'Sequence',
      'RecordReplay',
      'McpAutomationBridge_SequenceTakeRecorderSources.cpp',
    );
    const takeSourcePreparation = privateSource(
      'Domains',
      'Sequence',
      'RecordReplay',
      'McpAutomationBridge_SequenceTakeRecorderSourcePreparation.cpp',
    );
    const settings = publicSource('McpAutomationBridgeSettings.h');

    expect(executor).toContain('MovieRenderExecutorClassAllowlist');
    expect(executor).toContain('MRQ_EXECUTOR_NOT_ALLOWED');
    expect(executor).toContain('ValidateExecutorClassAllowlist');
    expect(settings).toContain('TakeRecorderSourceClassAllowlist');
    expect(takeSources).toContain('PrepareTakeRecorderSources');
    expect(takeSourcePreparation).toContain(
      'TakeRecorderSourceClassAllowlist',
    );
    expect(takeSourcePreparation).toContain(
      'source class is not allowlisted',
    );
    expect(takeSourcePreparation).toContain('MaxTakeRecorderSourceItems');
    expect(takeSourcePreparation).toContain('MaxTakeRecorderStringLength');
    const burnIns = privateSource(
      'Domains',
      'Sequence',
      'MovieRender',
      'McpAutomationBridge_SequenceMovieRenderBurnIns.cpp',
    );
    expect(burnIns).toContain('MovieRenderBurnInClassAllowlist');
    expect(burnIns).toContain('BURN_IN_CLASS_NOT_ALLOWED');
  });

  it('proves an allowlisted burn-in through executor output data', () => {
    const happyCases = readFileSync(
      resolve(
        process.cwd(),
        'tests',
        'mcp-tools',
        'utility',
        'cinematics-cases',
        'movie-render.cjs',
      ),
      'utf8',
    );
    const outputProof = privateSource(
      'Domains',
      'Sequence',
      'MovieRender',
      'McpAutomationBridge_SequenceMovieRenderOutputProof.cpp',
    );

    expect(happyCases).toContain('DefaultBurnIn.DefaultBurnIn_C');
    expect(happyCases).toContain("includes: 'BurnInOverlay'");
    expect(outputProof).toContain('ReportedRenderPasses.Add');
    expect(outputProof).toContain('TEXT("renderPasses")');
  });

  it('exposes Movie Render Queue allowlisted executor contract from case aggregation', () => {
    const happyCases = readFileSync(
      resolve(
        process.cwd(),
        'tests',
        'mcp-tools',
        'utility',
        'cinematics-cases',
        'movie-render.cjs',
      ),
      'utf8',
    );
    const executor = privateSource(
      'Domains',
      'Sequence',
      'MovieRender',
      'McpAutomationBridge_SequenceMovieRenderExecutor.cpp',
    );

    expect(happyCases).toContain('MoviePipelineInProcessExecutor');
    expect(happyCases).toContain(
      '/MovieRenderPipeline/Blueprints/DefaultBurnIn.DefaultBurnIn_C',
    );
    expect(executor).toContain('MovieRenderExecutorClassAllowlist');
  });

  it('rejects non-post-process materials before adding custom render passes', () => {
    const passes = privateSource(
      'Domains',
      'Sequence',
      'MovieRender',
      'McpAutomationBridge_SequenceMovieRenderPasses.cpp',
    );
    const happyCases = readFileSync(
      resolve(
        process.cwd(),
        'tests',
        'mcp-tools',
        'utility',
        'cinematics-cases',
        'movie-render.cjs',
      ),
      'utf8',
    );
    const nativeCases = readFileSync(
      resolve(
        process.cwd(),
        'scripts',
        'sequence-native-direct-render-job.mjs',
      ),
      'utf8',
    );
    const adversarialCases = readFileSync(
      resolve(
        process.cwd(),
        'tests',
        'mcp-tools',
        'utility',
        'cinematics-cases',
        'adversarial-render-media-cases.cjs',
      ),
      'utf8',
    );

    expect(passes).toContain('MD_PostProcess');
    expect(passes).toContain('RENDER_PASS_MATERIAL_DOMAIN_INVALID');
    expect(happyCases).toContain(
      '/Engine/BufferVisualization/CustomStencil.CustomStencil',
    );
    expect(nativeCases).toContain(
      '/Engine/BufferVisualization/CustomStencil.CustomStencil',
    );
    expect(adversarialCases).toContain(
      '/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial',
    );
    expect(adversarialCases).toContain(
      'render_pass_material_domain_invalid',
    );
    expect(nativeCases).toContain('RENDER_PASS_MATERIAL_DOMAIN_INVALID');
  });

  it('bounds render output discovery', () => {
    const outputProof = privateSource(
      'Domains',
      'Sequence',
      'MovieRender',
      'McpAutomationBridge_SequenceMovieRenderOutputProof.cpp',
    );

    expect(outputProof).not.toContain('FindFilesRecursive');
    expect(outputProof).toContain('MaxMovieRenderOutputScanFiles');
    expect(outputProof).toContain('MRQ_OUTPUT_SCAN_LIMIT_EXCEEDED');
    expect(outputProof).toContain('MakeOutputFileAlias');
    expect(outputProof).toContain('TEXT("/Saved/")');
  });

  it('enforces strict native manage_sequence arguments', () => {
    const definition = privateSource(
      'MCP',
      'Tools',
      'Utility',
      'McpTool_ManageSequence.cpp',
    );
    const transport = privateSource(
      'MCP',
      'Transport',
      'McpNativeTransportJsonRpc.cpp',
    );

    expect(definition).toContain('EnforceStrictArguments() const override');
    expect(transport).toContain('ValidateToolArguments(');

    const validation = privateSource(
      'MCP',
      'Transport',
      'McpNativeTransportArgumentValidation.cpp',
    );
    const schemaFields = privateSource(
      'MCP',
      'Tools',
      'Utility',
      'McpTool_ManageSequenceSchemaFields.cpp',
    );
    expect(validation).toContain('ValidateValueAgainstSchema');
    expect(validation).toContain('INVALID_TOOL_ARGUMENT');
    expect(validation).toContain('UNKNOWN_TOOL_ARGUMENT');
    expect(validation).toContain('FMath::TruncToDouble');
    expect(validation).toContain('9007199254740991.0');
    expect(validation).not.toContain('FMath::IsNearlyEqual');
    expect(schemaFields).toContain('.Object(TEXT("settings")');
    expect(schemaFields).not.toContain(
      '.FreeformObject(TEXT("settings")',
    );
    expect(schemaFields).toContain('.Object(TEXT("burnIn")');
    expect(validation).toContain('case EJson::Null:');
  });

  it('bounds native sessions, pending calls, and the editor request queue', () => {
    const transport = privateSource(
      'MCP',
      'Transport',
      'McpNativeTransport.h',
    );
    const subsystem = publicSource('McpAutomationBridgeSubsystem.h');
    const queue = privateSource(
      'Core',
      'Subsystem',
      'McpAutomationBridgeSubsystemRequestQueue.cpp',
    );

    expect(transport).toContain('MaxActiveSessions = 16');
    expect(transport).toContain(
      'AbandonedSessionGraceSeconds = 5.0',
    );
    expect(transport).toContain('MaxPendingToolCalls = 16');
    expect(transport).toContain('MaxPendingToolCallsPerSession = 4');
    expect(transport).toContain('MaxClientToolCallsPerMinute = 120');
    expect(transport).toContain('TMap<FString, FClientRateState>');
    expect(subsystem).toContain('MaxPendingAutomationRequests = 64');
    expect(subsystem).toContain('MaxAutomationRequestsPerTick = 16');
    expect(queue).toContain('TEXT("Automation request queue is full');
    expect(queue).toContain('PendingAutomationRequests.RemoveAt(');
  });

  it('requires explicit opt-in for native non-loopback binding and serializes local tool mutation with session deletion', () => {
    const lifecycle = privateSource(
      'MCP',
      'Transport',
      'McpNativeTransportLifecycle.cpp',
    );
    const jsonRpc = privateSource(
      'MCP',
      'Transport',
      'McpNativeTransportDynamicTools.cpp',
    );

    expect(lifecycle).toContain('InListenHost');
    expect(lifecycle).toContain('bInAllowNonLoopback');
    expect(lifecycle).toContain('!bIsLoopback && !bAllowNonLoopback');
    expect(lifecycle).toContain('falling back to 127.0.0.1');
    expect(lifecycle).toContain('explicitly allowed non-loopback address');
    expect(jsonRpc).toContain('FScopeLock SessionLock(&SessionMutex)');
    expect(jsonRpc).toContain('ActiveSessions.Contains(SessionId)');
    expect(jsonRpc.indexOf('ActiveSessions.Contains(SessionId)')).toBeLessThan(
      jsonRpc.indexOf('ToolManager.HandleAction(Action, Arguments)'),
    );
  });

  it('coalesces native progress writes per SSE request', () => {
    const transport = privateSource(
      'MCP',
      'Transport',
      'McpNativeTransport.h',
    );
    const pending = privateSource(
      'MCP',
      'Transport',
      'McpNativeTransportPendingRequests.cpp',
    );
    const nativeScenario = nativeSchemaScenario();

    expect(transport).toContain(
      'std::atomic<bool> bProgressWritePending{false}',
    );
    const progressFunction = pending.slice(
      pending.indexOf('void FMcpNativeTransport::SendSSEProgressUpdate'),
    );
    expect(progressFunction).toContain('compare_exchange_strong');
    expect(progressFunction).toContain('bProgressWritePending.store(false)');
    expect(progressFunction.indexOf('compare_exchange_strong')).toBeLessThan(
      progressFunction.indexOf('PendingAsyncWrites.fetch_add(1)'),
    );
    expect(nativeScenario).toContain('reject fractional native render width');
    expect(nativeScenario).toContain('reject fractional native start frame');
  });

  it('declares frame and resolution inputs as integers in both schemas', () => {
    const typeScript = readFileSync(
      resolve(
        process.cwd(),
        'src',
        'tools',
        'definitions',
        'utility',
        'manage-sequence-tool.ts',
      ),
      'utf8',
    );
    const native = privateSource(
      'MCP',
      'Tools',
      'Utility',
      'McpTool_ManageSequenceSchemaFields.cpp',
    );

    for (const field of [
      'frame',
      'width',
      'height',
      'startFrame',
      'endFrame',
      'lengthInFrames',
      'playbackStart',
      'playbackEnd',
    ]) {
      expect(typeScript).toContain(`${field}: commonSchemas.integerProp`);
      expect(native).toContain(`.Integer(TEXT("${field}")`);
      expect(native).not.toContain(`.Number(TEXT("${field}")`);
    }
  });

  it('recycles only unused native sessions and protects active clients', () => {
    const discovery = privateSource(
      'MCP',
      'Transport',
      'McpNativeTransportToolDiscovery.cpp',
    );

    expect(discovery).toContain('EvictedSessionId');
    expect(discovery).toContain('!RateState->bHasClientActivity');
    expect(discovery).toContain('InitializationCompletedAt');
    expect(discovery).toContain('AbandonedSessionGraceSeconds');
    expect(discovery).toContain('ActiveSessions.Remove(EvictedSessionId)');
    expect(discovery).toContain('TEXT("Native MCP session limit reached")');
    expect(discovery).not.toContain('EvictedSessionId = OldestSessionId');
  });

  it('atomically reserves notification streams and tears down sessions', () => {
    const transport = privateSource(
      'MCP',
      'Transport',
      'McpNativeTransport.h',
    );
    const notifications = privateSource(
      'MCP',
      'Transport',
      'McpNativeTransportNotifications.cpp',
    );
    const sessions = privateSource(
      'MCP',
      'Transport',
      'McpNativeTransportSessions.cpp',
    );
    const queueCancellation = privateSource(
      'Core',
      'Subsystem',
      'McpAutomationBridgeSubsystemRequestQueueCancellation.cpp',
    );

    expect(transport).toContain('std::atomic<bool> bReady{false}');
    expect(notifications).toContain(
      'NotificationStreams.Add(Stream->StreamId, Stream)',
    );
    expect(notifications.indexOf(
      'NotificationStreams.Add(Stream->StreamId, Stream)',
    )).toBeLessThan(notifications.indexOf(
      'SendSSEHeaders(\n\t\t\t\tStream->Socket, SessionId, CorsOrigin)',
    ));
    expect(notifications).toContain(
      'FScopeLock SessionLock(&SessionMutex)',
    );
    expect(notifications).toContain(
      'FScopeLock StreamLock(&NotificationStreamsMutex)',
    );
    expect(notifications).toContain(
      'FScopeLock WriteLock(&Stream->WriteMutex)',
    );
    expect(notifications).toContain('Stream->Socket = nullptr');
    expect(notifications).toContain(
      'NotificationStreams.Remove(Stream->StreamId)',
    );
    expect(sessions).toContain('CloseSessionConnections');
    expect(sessions).toContain('SSEConnections.CreateIterator()');
    expect(sessions).toContain('CancelAutomationRequests(RequestIds)');
    expect(sessions).not.toContain('CancelAutomationRequest(RequestId)');
    expect(queueCancellation).toContain('CancelAutomationRequests');
    expect(queueCancellation).toContain('AutomationRequestExecutionMutex');
    expect(queueCancellation).toContain('CanceledAutomationRequestIds');
    expect(queueCancellation).toContain('InFlightAutomationRequestIds');
    expect(queueCancellation).toContain('AutomationRequestCancellationCallbacks');
    expect(queueCancellation.indexOf('CanceledAutomationRequestIds.Add(RequestId)')).toBeLessThan(
      queueCancellation.indexOf('FScopeLock ExecutionLock(&AutomationRequestExecutionMutex)'),
    );
  });

  it('cancels every queued and asynchronous request before native shutdown closes streams', () => {
    const subsystem = publicSource('McpAutomationBridgeSubsystem.h');
    const queue = privateSource(
      'Core',
      'Subsystem',
      'McpAutomationBridgeSubsystemRequestQueue.cpp',
    );
    const queueCancellation = privateSource(
      'Core',
      'Subsystem',
      'McpAutomationBridgeSubsystemRequestQueueCancellation.cpp',
    );
    const subsystemLifecycle = privateSource(
      'Core',
      'Subsystem',
      'McpAutomationBridgeSubsystemLifecycle.cpp',
    );
    const lifecycle = privateSource(
      'MCP',
      'Transport',
      'McpNativeTransportLifecycle.cpp',
    );
    const tests = privateSource(
      'Core',
      'Subsystem',
      'McpAutomationBridgeSubsystemRequestQueueTests.cpp',
    );

    expect(subsystem).toContain('CancelAllAutomationRequests');
    expect(subsystem).toContain('bAcceptingAutomationRequests');
    expect(queueCancellation).toContain(
      'UMcpAutomationBridgeSubsystem::CancelAllAutomationRequests',
    );
    expect(queue).toContain('StopAcceptingAutomationRequests');
    expect(queue).toContain('if (!bAcceptingAutomationRequests)');
    expect(queueCancellation).toContain(
      'AutomationRequestCancellationCallbacks.GenerateKeyArray',
    );
    expect(subsystemLifecycle.indexOf(
      'StopAcceptingAutomationRequests()',
    )).toBeLessThan(subsystemLifecycle.indexOf(
      'RemoveTicker',
    ));
    expect(subsystemLifecycle).toContain('CancelAllAutomationRequests()');
    expect(lifecycle.match(
      /Subsystem->CancelAllAutomationRequests\(\)/g,
    )).toHaveLength(1);
    expect(lifecycle.lastIndexOf(
      'Subsystem->CancelAllAutomationRequests()',
    )).toBeLessThan(lifecycle.indexOf(
      '// Close all active SSE connections with error.',
    ));
    expect(tests).toContain('ShutdownCancellation');
    expect(tests).toContain('queued request is not dispatched');
    expect(tests).toContain('late request admission is rejected');
    expect(tests).toContain('asynchronous cancellation callback runs');
  });

  it('validates every sequence integer at exact int32 boundaries before dispatch', () => {
    const handler = privateSource(
      'Domains',
      'Sequence',
      'McpAutomationBridge_SequenceHandlers.cpp',
    );
    const validation = privateSource(
      'Domains',
      'Sequence',
      'Validation',
      'McpAutomationBridge_SequenceIntegerValidation.cpp',
    );
    const validationTests = privateSource(
      'Domains',
      'Sequence',
      'Validation',
      'McpAutomationBridge_SequenceIntegerValidationTests.cpp',
    );
    const mediaOpen = privateSource(
      'Domains',
      'Sequence',
      'Media',
      'McpAutomationBridge_SequenceMediaPlaybackOpen.cpp',
    );
    const nativeScenario = nativeSchemaScenario();

    expect(handler).toContain('ValidateSequenceIntegerFields');
    expect(handler.indexOf('ValidateSequenceIntegerFields')).toBeLessThan(
      handler.indexOf('HandleSequenceCreate('),
    );
    expect(validation).toContain('MIN_int32');
    expect(validation).toContain('MAX_int32');
    expect(validation).toContain('FMath::TruncToDouble');
    expect(validation).toContain('playlistIndex');
    expect(validation).toContain('startFrame');
    expect(validation).toContain('durationFrames');
    expect(validation).toContain('rowIndex');
    expect(validationTests).toContain('MIN_int32');
    expect(validationTests).toContain('MAX_int32');
    expect(validationTests).toContain('one below int32');
    expect(validationTests).toContain('one above int32');
    expect(mediaOpen).not.toContain(
      'static_cast<int32>(GetNumberAny(',
    );
    expect(nativeScenario).toContain('accept native int32 maximum');
    expect(nativeScenario).toContain('accept native int32 minimum');
    expect(nativeScenario).toContain('reject native int32 positive overflow');
    expect(nativeScenario).toContain('reject native int32 negative overflow');
  });

  it('checks derived frame conversion and arithmetic before constructing ranges', () => {
    const frameMath = privateSource(
      'Domains',
      'Sequence',
      'Validation',
      'McpAutomationBridge_SequenceFrameMath.cpp',
    );
    const frameMathTests = privateSource(
      'Domains',
      'Sequence',
      'Validation',
      'McpAutomationBridge_SequenceFrameMathTests.cpp',
    );
    const cinematics = privateSource(
      'Domains',
      'Sequence',
      'Cinematics',
      'McpAutomationBridge_SequenceCinematics.cpp',
    );
    const properties = privateSource(
      'Domains',
      'Sequence',
      'McpAutomationBridge_SequenceHandlersProperties.cpp',
    );
    const ranges = privateSource(
      'Domains',
      'Sequence',
      'McpAutomationBridge_SequenceHandlersRanges.cpp',
    );
    const nativeRender = readFileSync(
      resolve(
        process.cwd(),
        'scripts',
        'sequence-native-direct-render-sequence.mjs',
      ),
      'utf8',
    );

    expect(frameMath).toContain('TryTransformFrame');
    expect(frameMath).toContain('TryTransformFrameFloor');
    expect(frameMath).toContain('TryAddFrames');
    expect(frameMath).toContain('TrySubtractFrames');
    expect(frameMath).toContain('TrySecondsToFrame');
    expect(frameMath).toContain('MIN_int32');
    expect(frameMath).toContain('MAX_int32');
    expect(cinematics).toContain('ValidateCinematicFrameRequest');
    expect(properties).toContain('McpSequenceFrameMath::TryAddFrames');
    expect(ranges).toContain('McpSequenceFrameMath::TrySecondsToFrame');
    expect(frameMathTests).toContain('transformed maximum is rejected');
    expect(frameMathTests).toContain(
      'fractional keyframe conversion floors',
    );
    expect(frameMathTests).toContain('range endpoint overflow is rejected');
    expect(nativeRender).toContain('reject native derived frame overflow');
  });

  it('keeps native start_render requests alive for their render and cancellation budget', () => {
    const transport = privateSource(
      'MCP',
      'Transport',
      'McpNativeTransport.h',
    );
    const jsonRpc = privateSource(
      'MCP',
      'Transport',
      'McpNativeTransportJsonRpc.cpp',
    );
    const policy = privateSource(
      'MCP',
      'Transport',
      'McpNativeTransportTimeoutPolicy.cpp',
    );
    const cleanup = privateSource(
      'MCP',
      'Transport',
      'McpNativeTransportCleanup.cpp',
    );

    expect(transport).toContain('double TimeoutSeconds');
    expect(policy).toContain('ResolveToolCallTimeoutSeconds');
    expect(policy).toContain('NativeResponseGraceMs');
    expect(policy).toContain('MaxMovieRenderCancellationWaitMs');
    expect(jsonRpc).toContain('MaxMovieRenderCancellationWaitMs');
    expect(jsonRpc).toContain('Conn->TimeoutSeconds');
    expect(cleanup).toContain('Conn->TimeoutSeconds');
    expect(cleanup).toContain('CancelAutomationRequest(Entry.Key)');
    expect(cleanup.indexOf('CancelAutomationRequest(Entry.Key)')).toBeLessThan(
      cleanup.indexOf('CompletePendingRequest(Entry.Key'),
    );
    expect(cleanup).not.toContain(
      'Now - Conn->StartTime > RequestTimeoutSeconds',
    );
  });

  it('registers cancellation for dispatched asynchronous sequence work', () => {
    const subsystem = publicSource('McpAutomationBridgeSubsystem.h');
    const render = privateSource(
      'Domains',
      'Sequence',
      'MovieRender',
      'McpAutomationBridge_SequenceMovieRenderExecution.cpp',
    );
    const media = privateSource(
      'Domains',
      'Sequence',
      'Media',
      'McpAutomationBridge_SequenceMediaPlaybackAsync.cpp',
    );
    const recording = privateSource(
      'Domains',
      'Sequence',
      'RecordReplay',
      'McpAutomationBridge_SequenceTakeRecorderResults.cpp',
    );

    expect(subsystem).toContain('RegisterAutomationRequestCancellation');
    expect(subsystem).toContain('ClearAutomationRequestCancellation');
    expect(render).toContain('RegisterAutomationRequestCancellation');
    expect(render).toContain('CancelStartRender');
    expect(media).toContain('RegisterAutomationRequestCancellation');
    expect(media).toContain('CancelMediaPlayback');
    expect(recording).toContain('RegisterAutomationRequestCancellation');
    expect(recording).toContain('CancelRecording()');
  });

  it('keeps native session identifiers out of logs and version metadata aligned', () => {
    const jsonRpc = privateSource(
      'MCP',
      'Transport',
      'McpNativeTransportJsonRpc.cpp',
    );
    const connection = privateSource(
      'MCP',
      'Transport',
      'McpNativeTransportConnection.cpp',
    );
    const cleanup = privateSource(
      'MCP',
      'Transport',
      'McpNativeTransportCleanup.cpp',
    );
    const notifications = privateSource(
      'MCP',
      'Transport',
      'McpNativeTransportNotifications.cpp',
    );
    const transport = privateSource(
      'MCP',
      'Transport',
      'McpNativeTransport.h',
    );
    const serverInfo = readFileSync(
      resolve(
        process.cwd(),
        'plugins',
        'McpAutomationBridge',
        'Resources',
        'MCP',
        'server-info.json',
      ),
      'utf8',
    );

    expect(jsonRpc).not.toContain('*OutSessionId');
    expect(connection).not.toContain('*HttpReq.SessionId');
    expect(cleanup).not.toContain('*Stream->SessionId');
    expect(notifications).not.toContain('*SessionId');
    expect(transport).toContain('ServerVersion = TEXT("0.5.31")');
    expect(JSON.parse(serverInfo).version).toBe('0.5.31');
  });

  it('bounds render filename zero padding', () => {
    const validation = privateSource(
      'Domains',
      'Sequence',
      'MovieRender',
      'McpAutomationBridge_SequenceMovieRenderOutputValidation.cpp',
    );
    const output = privateSource(
      'Domains',
      'Sequence',
      'MovieRender',
      'McpAutomationBridge_SequenceMovieRenderOutput.cpp',
    );

    expect(validation).toContain('MaxMovieRenderZeroPadFrameNumbers');
    expect(validation).toContain('INVALID_ZERO_PAD_FRAME_NUMBERS');
    expect(output).not.toContain(
      'Output->ZeroPadFrameNumbers = FMath::Max(1, Value)',
    );
  });
});
