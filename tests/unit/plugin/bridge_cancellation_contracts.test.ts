/// <reference types="node" />

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

// Source-contract tests for the TypeScript stdio MCP surface -> WebSocket
// automation bridge -> Unreal `cancel_request` forwarding (the counterpart to
// the native /mcp transport cancellation that was already implemented).
//
// These read the TS and C++ source and assert the required structure is
// present and wired. RED = the stdio surface dropped notifications/cancelled;
// GREEN = forwarding is registered, correlated, and routed to the plugin
// cancellation primitive. A live Unreal Editor is not required in CI.

const repoRoot = process.cwd();
const read = (rel: string): string =>
    readFileSync(resolve(repoRoot, rel), 'utf8');

const dispatcher = read('src/automation/bridge-request-dispatcher.ts');
const correlation = read('src/automation/request-correlation.ts');
const types = read('src/automation/types.ts');
const serverFactory = read('src/server/server-factory.ts');
const toolRegistry = read('src/server/tool-registry.ts');
const dispatch = read('src/tools/handlers/foundation/dispatch/automation-request-dispatch.ts');
const connManagerH = read('plugins/McpAutomationBridge/Source/McpAutomationBridge/Public/McpConnectionManager.h');
const messagesCpp = read('plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Transport/Connection/McpConnectionManagerMessages.cpp');
const cancellationCpp = read('plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Transport/Connection/McpConnectionManagerCancellation.cpp');
const lifecycleCpp = read('plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Core/Subsystem/McpAutomationBridgeSubsystemLifecycle.cpp');
const subsystemH = read('plugins/McpAutomationBridge/Source/McpAutomationBridge/Public/McpAutomationBridgeSubsystem.h');

const pureLoc = (source: string): number =>
    source
        .split('\n')
        .filter((line) => {
            const trimmed = line.trim();
            return trimmed.length > 0 && !trimmed.startsWith('//') && !trimmed.startsWith('*');
        }).length;

describe('TS stdio notifications/cancelled forwarding', () => {
    it('registers CancelledNotificationSchema and forwards to the bridge cancel primitive', () => {
        expect(serverFactory).toContain('CancelledNotificationSchema');
        expect(serverFactory).toContain('setNotificationHandler(CancelledNotificationSchema');
        expect(serverFactory).toContain('automationBridge.cancelMcpRequest(');
        expect(serverFactory).toContain('canonicalizeMcpRequestId(');
    });

    it('captures extra.requestId and extra.signal in both gateway and legacy paths', () => {
        expect(toolRegistry).toContain('async (request, extra) =>');
        expect(toolRegistry).toContain('canonicalizeMcpRequestId(extra.requestId)');
        expect(toolRegistry).toContain('runWithMcpRequestContext(');
        expect(toolRegistry).toContain('this.automationBridge.cancelMcpRequest(');
    });

    it('threads the active request context into executeAutomationRequest', () => {
        expect(dispatch).toContain('getMcpRequestContext()');
        expect(dispatch).toContain('mcpRequestId');
    });
});

describe('Automation bridge cancellation correlation + wire behavior', () => {
    it('correlates one MCP id to many automation ids and supports cancel/cleanup', () => {
        expect(dispatcher).toContain('cancelMcpRequest(');
        expect(correlation).toContain('class RequestCorrelation');
        expect(correlation).toContain('register(');
        expect(correlation).toContain('settle(');
        expect(correlation).toContain('queuedByMcp');
        expect(correlation).toContain('byMcp');
        expect(correlation).toContain('byAuto');
    });

    it('sends a targeted cancel_request frame per correlated automation id', () => {
        expect(dispatcher).toContain("type: 'cancel_request'");
        expect(dispatcher).toContain('McpRequestCancelledError');
        expect(correlation).toContain('cancel(');
        expect(correlation).toContain('sendFrame');
    });

    it('declares the CancelRequestMessage wire type and queues the mcpRequestId', () => {
        expect(types).toContain("type: 'cancel_request'");
        expect(types).toContain('CancelRequestMessage');
        expect(types).toContain('mcpRequestId?: string;');
    });
});

describe('Plugin WebSocket cancel_request routing', () => {
    it('declares the cancellation delegate and handler on the connection manager', () => {
        expect(connManagerH).toContain('FMcpRequestCancelledCallback');
        expect(connManagerH).toContain('SetOnAutomationRequestCancelled(');
        expect(connManagerH).toContain('HandleCancelRequest(');
    });

    it('routes inbound cancel_request frames to HandleCancelRequest', () => {
        expect(messagesCpp).toContain('TEXT("cancel_request")');
        expect(messagesCpp).toContain('HandleCancelRequest(');
    });

    it('implements the cancellation handler and wires it to the subsystem primitive', () => {
        expect(cancellationCpp).toContain('FMcpConnectionManager::SetOnAutomationRequestCancelled(');
        expect(cancellationCpp).toContain('FMcpConnectionManager::HandleCancelRequest(');
        expect(cancellationCpp).toContain('OnAutomationRequestCancelled.Execute(RequestId)');
        expect(lifecycleCpp).toContain('SetOnAutomationRequestCancelled(');
        expect(lifecycleCpp).toContain('CancelAutomationRequest(RequestId)');
    });

    it('reuses the existing subsystem CancelAutomationRequest primitive', () => {
        expect(subsystemH).toContain('bool CancelAutomationRequest(const FString& RequestId);');
    });

    it('keeps the new cancellation file within the 250 pure-LOC ceiling', () => {
        expect(pureLoc(cancellationCpp)).toBeLessThanOrEqual(250);
    });
});
