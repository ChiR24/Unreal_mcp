import { describe, expect, it } from 'vitest';
import { handleConsolidatedToolCall } from './consolidated-tool-handlers.js';
import { createConnectedTools } from './consolidated-tool-handlers.test-support.js';

describe('system_control screenshot routing', () => {
  it('routes full editor screenshot mode with base64 image return enabled', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleConsolidatedToolCall('system_control', {
      action: 'screenshot',
      filename: 'FullEditor',
      mode: 'full_editor_window'
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('control_editor', {
      action: 'screenshot',
      filename: 'FullEditor',
      resolution: undefined,
      mode: 'full_editor_window',
      returnBase64: true
    }, {});
  });

  it('forwards screenshot metadata opt-in for system control screenshots', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleConsolidatedToolCall('system_control', {
      action: 'screenshot',
      filename: 'FullEditor',
      mode: 'full_editor_window',
      includeMetadata: true
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('control_editor', {
      action: 'screenshot',
      filename: 'FullEditor',
      resolution: undefined,
      mode: 'full_editor_window',
      returnBase64: true,
      includeMetadata: true
    }, {});
  });
});

describe('system_control Unreal Insights routing', () => {
  it('routes Unreal Insights trace actions through manage_insights', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    const result = await handleConsolidatedToolCall('system_control', {
      action: 'capture_insights_trace',
      channels: 'cpu,gpu',
      traceFile: 'Saved/Profiling/McpTrace.utrace',
      overwrite: true
    }, tools) as Record<string, unknown>;

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_insights', expect.objectContaining({
      action: 'capture_insights_trace',
      subAction: 'capture_insights_trace',
      channels: 'cpu,gpu',
      traceFile: 'Saved/Profiling/McpTrace.utrace',
      overwrite: true
    }), expect.any(Object));
    expect(result).toMatchObject({
      success: true,
      action: 'capture_insights_trace',
      channels: 'cpu,gpu',
      sessionType: 'trace'
    });
  });

  it('does not synthesize empty Unreal Insights channels', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    const result = await handleConsolidatedToolCall('system_control', {
      action: 'start_unreal_insights',
      connectionType: 'file',
      traceFile: 'McpTrace.utrace',
      overwrite: true
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_insights', expect.objectContaining({
      action: 'start_unreal_insights',
      subAction: 'start_unreal_insights',
      traceFile: 'McpTrace.utrace',
      overwrite: true
    }), expect.any(Object));
    const firstCall = sendAutomationRequest.mock.calls[0];
    expect(firstCall).toBeDefined();
    expect(firstCall?.[1]).not.toHaveProperty('channels');
    expect(result).toMatchObject({
      success: true,
      action: 'start_unreal_insights',
      sessionType: 'trace'
    });
    expect(result).not.toHaveProperty('channels');
  });

  it('rejects unsafe Unreal Insights channels before bridge dispatch', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    const result = await handleConsolidatedToolCall('system_control', {
      action: 'start_unreal_insights',
      channels: 'cpu;quit'
    }, tools) as Record<string, unknown>;

    expect(sendAutomationRequest).not.toHaveBeenCalled();
    expect(result).toMatchObject({
      success: false,
      error: 'INVALID_CHANNELS'
    });
  });
});
