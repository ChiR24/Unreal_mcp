/// <reference types="node" />

import { readFileSync, readdirSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const pluginPrivateRoot = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private',
);
const sourceFilePattern = /\.(?:cpp|h)$/u;

const privateSource = (...parts: readonly string[]): string =>
  readFileSync(resolve(pluginPrivateRoot, ...parts), 'utf8');

const listFiles = (directory: string): readonly string[] => {
  const files: string[] = [];

  for (const entry of readdirSync(directory, { withFileTypes: true })) {
    const entryPath = resolve(directory, entry.name);
    if (entry.isDirectory()) {
      files.push(...listFiles(entryPath));
      continue;
    }
    files.push(entryPath);
  }

  return files;
};

describe('automation event stream contracts', () => {
  it('routes structured automation events through the subsystem broadcaster', () => {
    const bypasses = listFiles(pluginPrivateRoot)
      .filter((file) => sourceFilePattern.test(file))
      .filter((file) => {
        const source = readFileSync(file, 'utf8');
        return (
          source.includes('TEXT("automation_event")') &&
          source.includes('SendControlMessage(')
        );
      })
      .map((file) => file.replace(`${process.cwd()}/`, ''));

    expect(bypasses).toEqual([]);
  }, 60_000);

  it('sanitizes streamed log messages before broadcasting them', () => {
    const logHandlers = privateSource(
      'Domains',
      'Log',
      'McpAutomationBridge_LogHandlers.cpp',
    );
    const discovery = privateSource(
      'MCP',
      'Transport',
      'McpNativeTransportToolDiscovery.cpp',
    );

    expect(logHandlers).toContain('SanitizeEngineErrorForResponse(FString(V))');
    expect(logHandlers).toContain('.Left(2048)');
    expect(discovery).not.toContain('client: %s %s');
  });

  it('routes native system-control log subscriptions to the log handler', () => {
    const source = privateSource(
      'Domains',
      'SystemControl',
      'McpAutomationBridge_SystemControlHandlers.cpp',
    );

    expect(source).toContain('Lower == TEXT("subscribe")');
    expect(source).toContain('Lower == TEXT("unsubscribe")');
    expect(source).toContain(
      'HandleLogAction(RequestId, TEXT("manage_logs"), Payload',
    );
  });

  it('clears native log-event subscriptions with session lifecycle', () => {
    const sessions = privateSource(
      'MCP',
      'Transport',
      'McpNativeTransportSessions.cpp',
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
    const lifecycle = privateSource(
      'MCP',
      'Transport',
      'McpNativeTransportLifecycle.cpp',
    );

    expect(sessions).toContain('LogEventSubscribedSessions.Remove(SessionId)');
    expect(connection).toContain(
      'CloseSessionConnections(HttpReq.SessionId)',
    );
    expect(cleanup).toContain('CloseSessionConnections(SessionId)');
    expect(lifecycle).toContain('LogEventSubscribedSessions.Empty()');
  });

  it('keeps native log notifications log-only and off the caller thread', () => {
    const notifications = privateSource(
      'MCP',
      'Transport',
      'McpNativeTransportNotifications.cpp',
    );
    const writes = privateSource(
      'MCP',
      'Transport',
      'McpNativeTransportNotificationWrites.cpp',
    );

    expect(notifications).toContain('IsLogAutomationEvent');
    expect(notifications).toContain('EventName.Equals(TEXT("log")');
    expect(notifications).toContain('FMcpNativeTransport::BroadcastLogEventNotification');
    expect(notifications).toContain('return 0;');
    expect(notifications).toContain('QueueNotificationEventWrites');
    expect(writes).toContain('TryReserveAsyncNotificationWrite');
    expect(writes).toContain('compare_exchange_weak');
    expect(writes).toContain('MaxPendingNotificationWrites');
    expect(writes).toContain('Async(EAsyncExecution::ThreadPool');
    expect(writes).toContain('WriteNotificationEvent(*Stream, NotificationJson)');
    expect(writes).toContain('PendingAsyncWrites.fetch_sub(1)');
  });

  it('keeps WebSocket log subscription scoped to subscribed sockets', () => {
    const connectionHeader = readFileSync(
      resolve(
        process.cwd(),
        'plugins/McpAutomationBridge/Source/McpAutomationBridge/Public/McpConnectionManager.h',
      ),
      'utf8',
    );
    const responses = privateSource(
      'Transport',
      'Connection',
      'McpConnectionManagerResponses.cpp',
    );
    const socketEvents = privateSource(
      'Transport',
      'Connection',
      'McpConnectionManagerSocketEvents.cpp',
    );

    expect(connectionHeader).toContain('SetLogSubscription');
    expect(connectionHeader).toContain('HasLogSubscribers');
    expect(connectionHeader).toContain('LogSubscriberSockets');
    expect(responses).toContain('SendRawMessageToLogSubscribers');
    expect(responses).toContain('LogSubscriberSockets.Contains(Sock.Get())');
    expect(socketEvents).toContain('LogSubscriberSockets.Remove(Socket.Get())');
  });

  it('reconciles global log capture after implicit subscriber cleanup', () => {
    const subsystemHeader = readFileSync(
      resolve(
        process.cwd(),
        'plugins/McpAutomationBridge/Source/McpAutomationBridge/Public/McpAutomationBridgeSubsystem.h',
      ),
      'utf8',
    );
    const logHandlers = privateSource(
      'Domains',
      'Log',
      'McpAutomationBridge_LogHandlers.cpp',
    );
    const lifecycle = privateSource(
      'Core',
      'Subsystem',
      'McpAutomationBridgeSubsystemLifecycle.cpp',
    );
    const nativeConnection = privateSource(
      'MCP',
      'Transport',
      'McpNativeTransportConnection.cpp',
    );
    const nativeCleanup = privateSource(
      'MCP',
      'Transport',
      'McpNativeTransportCleanup.cpp',
    );
    const socketEvents = privateSource(
      'Transport',
      'Connection',
      'McpConnectionManagerSocketEvents.cpp',
    );

    expect(subsystemHeader).toContain('ReconcileLogCaptureDevice');
    expect(logHandlers).toContain(
      'UMcpAutomationBridgeSubsystem::ReconcileLogCaptureDevice',
    );
    expect(logHandlers).toContain('NativeTransport->HasLogEventSubscribers()');
    expect(logHandlers).toContain('ConnectionManager->HasLogSubscribers()');
    expect(logHandlers).toContain('GLog->RemoveOutputDevice(LogCaptureDevice.Get())');
    expect(logHandlers).toContain('LogCaptureDevice.Reset()');
    expect(logHandlers).toContain('ReconcileLogCaptureDevice();');
    expect(lifecycle).toContain('NativeTransport->CleanupStaleRequests();');
    expect(lifecycle).toContain('ReconcileLogCaptureDevice();');
    expect(nativeConnection).toContain(
      'CloseSessionConnections(HttpReq.SessionId)',
    );
    expect(nativeCleanup).toContain('CloseSessionConnections(SessionId)');
    expect(socketEvents).toContain('LogSubscriberSockets.Remove(Socket.Get())');
  });
});
