// Subscriptions to ue://changes: subscriptions/listen and resources/subscribe on native /mcp, and the changes_moved
// event the bridge pushes to a WebSocket client. Wiring and safety contracts only.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const PRIVATE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private');

/** Line comments removed, CRLF normalized, so no assertion can be satisfied by prose. */
const code = (...segments: readonly string[]): string =>
  readFileSync(join(PRIVATE, ...segments), 'utf8').replace(/\r\n/gu, '\n').replace(/\/\/[^\n]*/gu, ' ');

describe('native subscriptions to ue://changes', () => {
  const listen = code('MCP', 'Protocol', 'McpNativeTransportListen.cpp');
  const handler = listen.slice(listen.indexOf('void FMcpNativeTransport::HandleSubscriptionsListen('), listen.indexOf('FString FMcpNativeTransport::HandleResourceSubscription('));

  it('serves subscriptions/listen on the 2026-07-28 path and sweeps it from the keepalive thread', () => {
    expect(code('MCP', 'Protocol', 'McpNativeTransportModern.cpp')).toMatch(
      /Rpc\.Method == TEXT\("subscriptions\/listen"\)\)\s*\{\s*HandleSubscriptionsListen\(ClientSocket, HttpReq, Rpc, SessionId\);\s*return;/u,
    );
    expect(code('MCP', 'Transport', 'McpNativeTransportKeepalive.cpp')).toMatch(/SweepNotificationKeepalives\(\);\s*SweepChangeNotifications\(\);/u);
  });

  // A write-only or path-confined token must not hear of changes it may not read.
  it('gates every subscribed resource as reading it is, before the stream is admitted', () => {
    expect(handler.indexOf('McpAuthorizePrimitiveRead(Principal, Uri, Rpc.Id)')).toBeGreaterThan(-1);
    expect(handler.indexOf('McpAuthorizePrimitiveRead(')).toBeLessThan(handler.indexOf('NotificationStreams.Add('));
  });

  it('starts a stream from the current journal number, and answers its limit in band', () => {
    expect(handler).toContain('Stream->ChangesSeq = McpChangeJournal::LatestSeq();');
    expect(handler).toContain('FMcpJsonRpc::ErrorInternalError, TEXT("Subscription limit reached")');
    // Through the published stream, under its write lock: teardown may close that socket once the stream is listed.
    expect(handler.indexOf('SendSSEHeaders(Stream->Socket, FString(), HttpReq.Origin)')).toBeGreaterThan(-1);
  });

  it('ends each listen with its own result before the stream closes at shutdown', () => {
    const lifecycle = code('MCP', 'Transport', 'McpNativeTransportLifecycle.cpp');
    expect(lifecycle.indexOf('WriteNotificationEvent(*Stream, ListenEndFrame(Stream->ListenId))')).toBeGreaterThan(-1);
    expect(lifecycle.indexOf('ListenEndFrame(')).toBeLessThan(lifecycle.indexOf('CloseNotificationStream(Stream);'));
    expect(listen).toContain('Result->SetStringField(TEXT("resultType"), TEXT("complete"));');
  });

  it('answers resources/subscribe for earlier versions only, and advertises subscribe on both', () => {
    expect(code('MCP', 'Transport', 'McpNativeTransportPrimitives.cpp')).toContain(
      '(Method == TEXT("resources/subscribe") || Method == TEXT("resources/unsubscribe")) && !McpModern::Current().bActive',
    );
    expect(code('MCP', 'Transport', 'McpNativeTransportSessions.cpp')).toContain('ChangesSubscribedSessions.Remove(SessionId);');
    expect(code('MCP', 'Transport', 'McpNativeTransportToolDiscovery.cpp')).toMatch(
      /Resources->SetBoolField\(TEXT\("subscribe"\), true\);\s*Capabilities->SetObjectField\(TEXT\("resources"\), Resources\);/u,
    );
  });

  it('reads the journal from the transport threads only through what it publishes', () => {
    expect(code('Core', 'Journal', 'McpChangeJournal.cpp')).toMatch(/Entry\.LastSeq = \+\+State\.Seq;\s*PublishSeq\(State\.Seq\);/u);
    const events = code('Core', 'Journal', 'McpChangeJournalEvents.cpp');
    expect(events).toMatch(/FString LatestCursor\(uint64\* OutSeq\)\s*\{\s*FScopeLock Lock\(&GPublishedLock\);/u);
    expect(code('MCP', 'Resources', 'McpResourceReadContent.cpp')).toContain('McpChangeJournal::LatestCursor(&Seq)');
  });
});

describe('the bridge push of changes_moved', () => {
  const push = code('Transport', 'Connection', 'McpConnectionManagerChanges.cpp');

  it('runs from the manager tick, at most once a second, to principals that may read', () => {
    expect(code('Transport', 'Connection', 'McpConnectionManager.cpp')).toMatch(/FlushContentRootsUpdate\(\);\s*FlushChangesUpdate\(\);/u);
    expect(push).toContain('Now - LastChangesPushTime < ChangesPushIntervalSeconds');
    expect(push).toContain('Principal->IsScopeAuthorized(EMcpCapabilityScope::Read)');
    expect(push).toContain('TEXT("changes_moved")');
  });

  it('hands the cursor over at connect only to a principal that may read', () => {
    expect(code('Transport', 'Connection', 'McpConnectionManagerAuthority.cpp')).toMatch(
      /if \(Principal\.IsScopeAuthorized\(EMcpCapabilityScope::Read\)\)\s*\{\s*Ack->SetStringField\(TEXT\("changesCursor"\), McpChangeJournal::LatestCursor\(\)\);/u,
    );
  });
});
