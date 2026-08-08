import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

const pluginRoot = resolve(process.cwd(), 'plugins/McpAutomationBridge/Source');
const readPlugin = (relativePath: string) => readFileSync(resolve(pluginRoot, relativePath), 'utf8');

describe('Unreal MCP debug protocol contracts', () => {
  it('buffers handler responses until request-scoped diagnostics close', () => {
    const request = readPlugin('McpAutomationBridge/Private/Core/Requests/McpAutomationBridge_ProcessRequest.cpp');
    const responses = readPlugin('McpAutomationBridge/Private/Core/Subsystem/McpAutomationBridgeSubsystemResponses.cpp');
    expect(request).toContain('ON_SCOPE_EXIT');
    expect(request).toContain('CapturedErrors = EndErrorCapture();');
    expect(request).toContain('BeginErrorCapture();');
    expect(request).toContain('if (BufferedAutomationResponse.bValid)');
    expect(request).toContain('FBufferedAutomationResponse Response = BufferedAutomationResponse');
    expect(request).toContain('SendAutomationResponse(');
    expect(responses).toContain('BufferedAutomationResponse.bValid = true');
    expect(responses).toContain('bEffectiveSuccess = false');
    expect(responses).toContain('TEXT("ENGINE_ERROR")');
  });

  it('emits versioned correlated envelopes and Blueprint/test diagnostics', () => {
    const envelope = readPlugin('McpAutomationBridge/Private/Core/Subsystem/McpAutomationBridgeEventEnvelope.h');
    for (const field of ['type', 'sequence', 'timestamp', 'traceId', 'targetPid', 'frame', 'thread', 'eventCursor']) {
      expect(envelope).toContain(`TEXT("${field}")`);
    }
    const diagnostics = readPlugin('McpAutomationBridge/Private/Core/Subsystem/McpAutomationBridgeSubsystemDebugObservability.cpp');
    for (const field of ['blueprint_exception', 'graph', 'nodeGuid', 'scriptStack', 'automation_test_completed', 'warnings', 'errors', 'duration']) {
      expect(diagnostics).toContain(field);
    }
  });

  it('keeps probes bounded and excludes the runtime module from Shipping', () => {
    const runtime = readPlugin('McpDebugRuntime/Private/McpDebugRuntimeModule.cpp');
    const manifest = JSON.parse(readFileSync(resolve(process.cwd(), 'plugins/McpAutomationBridge/McpAutomationBridge.uplugin'), 'utf8'));
    expect(runtime).toContain('CaptureIntervalSeconds = 0.1');
    expect(runtime).toContain('MaxSnapshotBytes = 1024 * 1024');
    const module = manifest.Modules.find((entry: { Name: string }) => entry.Name === 'McpDebugRuntime');
    expect(module.TargetConfigurationAllowList).not.toContain('Shipping');
  });

  it('registers stopped trace output as a SHA-addressable artifact', () => {
    const service = readFileSync(resolve(process.cwd(), 'src/debug/debug-service.ts'), 'utf8');
    expect(service).toContain("'unreal_trace_stop'");
    expect(service).toContain("this.artifacts.registerFile(tracePath, 'unreal_trace'");
    expect(service).toContain("'abnormal_exit_manifest'");
  });
});
