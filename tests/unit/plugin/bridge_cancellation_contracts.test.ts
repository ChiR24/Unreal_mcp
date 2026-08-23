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
const socketEventsCpp = read('plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Transport/Connection/McpConnectionManagerSocketEvents.cpp');
const connectionCpp = read('plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Transport/Connection/McpConnectionManagerConnection.cpp');
const managerCpp = read('plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Transport/Connection/McpConnectionManager.cpp');
const subsystemH = read('plugins/McpAutomationBridge/Source/McpAutomationBridge/Public/McpAutomationBridgeSubsystem.h');
const nativeSessions = read('plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Transport/McpNativeTransportSessions.cpp');
const nativeGatewayStream = read('plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Transport/McpNativeTransportGatewayStream.cpp');

const pureLoc = (source: string): number =>
    source
        .split('\n')
        .filter((line) => {
            const trimmed = line.trim();
            return trimmed.length > 0 && !trimmed.startsWith('//') && !trimmed.startsWith('*');
        }).length;

// Scope a required pattern to the function that must contain it (e.g. the purge must live inside HandleClosed).
const splitFunctions = (source: string): Map<string, string> => {
    const map = new Map<string, string>();
    const re = /FMcpConnectionManager::(\w+)\s*\(/g;
    const matches = [...source.matchAll(re)];
    for (let i = 0; i < matches.length; i++) {
        const name = matches[i][1];
        const start = matches[i].index ?? 0;
        const end = i + 1 < matches.length ? (matches[i + 1].index ?? source.length) : source.length;
        map.set(name, source.slice(start, end));
    }
    return map;
};

const authSocketsAccessPattern = /AuthenticatedSockets\.(?:Add|Remove|Contains|Empty)/g;

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

describe('Plugin WebSocket cancellation synchronization + ownership', () => {
    it('declares a dedicated AuthSocketsMutex guarding AuthenticatedSockets', () => {
        expect(connManagerH).toContain('AuthSocketsMutex');
    });

    it('guards AuthenticatedSockets teardown with AuthSocketsMutex in ForceReconnect and Stop', () => {
        const connectionFns = splitFunctions(connectionCpp);
        const managerFns = splitFunctions(managerCpp);
        const forceReconnect = connectionFns.get('ForceReconnect') ?? '';
        const stop = managerFns.get('Stop') ?? '';
        expect(forceReconnect).toContain('AuthenticatedSockets');
        expect(forceReconnect).toContain('AuthSocketsMutex');
        expect(forceReconnect).toContain('FScopeLock');
        expect(stop).toContain('AuthenticatedSockets');
        expect(stop).toContain('AuthSocketsMutex');
        expect(stop).toContain('FScopeLock');
    });

    it('guards the cancellation callback authentication lookup in a short AuthSocketsMutex scope', () => {
        const cancellationFns = splitFunctions(cancellationCpp);
        const handleCancel = cancellationFns.get('HandleCancelRequest') ?? '';
        expect(handleCancel.match(authSocketsAccessPattern)).toHaveLength(1);
        expect(handleCancel).toMatch(
            /\{\s*FScopeLock Lock\(&AuthSocketsMutex\);\s*bIsAuthenticated = AuthenticatedSockets\.Contains\(Socket\.Get\(\)\);\s*\}/,
        );
    });

    it('guards all three message callback authentication accesses in short AuthSocketsMutex scopes', () => {
        const messageFns = splitFunctions(messagesCpp);
        const handleMessage = messageFns.get('HandleMessage') ?? '';
        expect(handleMessage.match(authSocketsAccessPattern)).toHaveLength(3);
        expect(handleMessage).toMatch(
            /\{\s*FScopeLock Lock\(&AuthSocketsMutex\);\s*bIsAuthenticated = AuthenticatedSockets\.Contains\(SocketPtr\);\s*\}/,
        );
        expect(handleMessage).toMatch(
            /\{\s*FScopeLock Lock\(&AuthSocketsMutex\);\s*AuthenticatedSockets\.Remove\(SocketPtr\);\s*\}/,
        );
        expect(handleMessage).toMatch(
            /\{\s*FScopeLock Lock\(&AuthSocketsMutex\);\s*AuthenticatedSockets\.Add\(SocketPtr\);\s*\}/,
        );
    });

    it('guards each socket-event callback authentication removal in a short AuthSocketsMutex scope', () => {
        const socketFns = splitFunctions(socketEventsCpp);
        const handleClientConnected = socketFns.get('HandleClientConnected') ?? '';
        const handleConnError = socketFns.get('HandleConnectionError') ?? '';
        const handleClosed = socketFns.get('HandleClosed') ?? '';
        expect(handleClientConnected.match(authSocketsAccessPattern)).toHaveLength(1);
        expect(handleConnError.match(authSocketsAccessPattern)).toHaveLength(1);
        expect(handleClosed.match(authSocketsAccessPattern)).toHaveLength(1);
        expect(handleClientConnected).toMatch(
            /\{\s*FScopeLock Lock\(&AuthSocketsMutex\);\s*AuthenticatedSockets\.Remove\(ClientSocket\.Get\(\)\);\s*\}/,
        );
        expect(handleConnError).toMatch(
            /\{\s*FScopeLock Lock\(&AuthSocketsMutex\);\s*AuthenticatedSockets\.Remove\(Socket\.Get\(\)\);\s*\}/,
        );
        expect(handleClosed).toMatch(
            /\{\s*FScopeLock Lock\(&AuthSocketsMutex\);\s*AuthenticatedSockets\.Remove\(Socket\.Get\(\)\);\s*\}/,
        );
    });

    it('purges the closing socket pending-request mappings in HandleClosed and HandleConnectionError', () => {
        const socketFns = splitFunctions(socketEventsCpp);
        const handleClosed = socketFns.get('HandleClosed') ?? '';
        const handleConnError = socketFns.get('HandleConnectionError') ?? '';
        expect(handleClosed).toContain('PendingRequestsToSockets');
        expect(handleClosed).toContain('PendingRequestsMutex');
        expect(handleConnError).toContain('PendingRequestsToSockets');
        expect(handleConnError).toContain('PendingRequestsMutex');
    });

    it('keeps every Connection shard within the 250 pure-LOC ceiling', () => {
        expect(pureLoc(socketEventsCpp)).toBeLessThanOrEqual(250);
        expect(pureLoc(connectionCpp)).toBeLessThanOrEqual(250);
        expect(pureLoc(managerCpp)).toBeLessThanOrEqual(250);
        expect(pureLoc(messagesCpp)).toBeLessThanOrEqual(250);
    });
});

describe('Native /mcp precise queue refusal (BB-003)', () => {
    it('surfaces the real EAutomationQueueRejection from native queue admission', () => {
        expect(nativeSessions).toContain('EAutomationQueueRejection& OutRejection');
        expect(nativeSessions).toContain('Subsystem->QueueAutomationRequest(');
        expect(nativeSessions).toContain('EAutomationQueueRejection::None');
    });

    it('maps SessionQueueFull to a code distinct from QueueFull in GatewayStream', () => {
        expect(nativeGatewayStream).toContain('EAutomationQueueRejection::SessionQueueFull');
        expect(nativeGatewayStream).toContain('AUTOMATION_SESSION_QUEUE_FULL');
        expect(nativeGatewayStream).toContain('EAutomationQueueRejection::QueueFull');
        expect(nativeGatewayStream).toContain('AUTOMATION_QUEUE_FULL');
    });

    it('maps NotAccepting and AlreadyCanceled to their own codes', () => {
        expect(nativeGatewayStream).toContain('EAutomationQueueRejection::NotAccepting');
        expect(nativeGatewayStream).toContain('AUTOMATION_NOT_ACCEPTING');
        expect(nativeGatewayStream).toContain('EAutomationQueueRejection::AlreadyCanceled');
        expect(nativeGatewayStream).toContain('AUTOMATION_ALREADY_CANCELED');
    });
});
