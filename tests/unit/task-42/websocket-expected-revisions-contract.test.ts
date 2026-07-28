import { readFileSync } from 'node:fs';

import { describe, expect, it } from 'vitest';

const ROOT = process.cwd();
const source = (relative: string): string => readFileSync(`${ROOT}/${relative}`, 'utf8');

describe('Task 42 WebSocket expected-revisions transport', () => {
  it('keeps pins typed and outside the action payload across the socket callback', () => {
    const managerHeader = source(
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Public/McpConnectionManager.h'
    );
    const messages = source(
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Transport/Connection/McpConnectionManagerMessages.cpp'
    );
    const lifecycle = source(
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Core/Subsystem/McpAutomationBridgeSubsystemLifecycle.cpp'
    );

    expect(managerHeader).toContain('DECLARE_DELEGATE_FiveParams(FMcpMessageReceivedCallback');
    expect(messages).toContain('FMcpLiveStateRevisions::ParseExpectedRevisions');
    expect(messages).toMatch(/OnMessageReceived\.Execute\([\s\S]*ExpectedRevisions\);/u);
    expect(lifecycle).toMatch(/QueueAutomationRequest\([\s\S]*ExpectedRevisions\);/u);
  });

  it('shares one parser between native HTTP and WebSocket ingress', () => {
    const native = source(
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Execute/McpNativeGatewayExpectedRevisions.cpp'
    );
    const messages = source(
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Transport/Connection/McpConnectionManagerMessages.cpp'
    );

    expect(native).toContain('FMcpLiveStateRevisions::ParseExpectedRevisions');
    expect(messages).toContain('FMcpLiveStateRevisions::ParseExpectedRevisions');
  });

  it('adds one strict live revision snapshot at the shared WebSocket response serializer', () => {
    const responses = source(
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Transport/Connection/McpConnectionManagerResponses.cpp'
    );
    const revisions = source(
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Foundation/McpLiveStateRevisions.cpp'
    );

    expect(responses).toMatch(/SetObjectField\(TEXT\("liveRevisions"\),[\s\S]*Snapshot/u);
    expect(revisions).toContain('FMcpLiveStateRevisionSnapshot::ToJson');
    expect(revisions).toContain('FMcpLiveStateRevisions::Snapshot');
  });
});
