// tests/unit/task-49/driver-offline.test.ts
// Task 49 — offline tests for BOTH real drivers.
//
// These never touch an editor. Every HTTP call and every child process is
// injected, so the exact bytes each driver puts on the wire are observable and
// asserted: framing, auth, protocol-version negotiation, SSE fragmentation,
// cancellation, shutdown and redaction.
//
// The fragmentation cases matter most. Every prior probe in scripts/qa/ parses SSE
// by splitting a fully-buffered body on "data: ", which silently assumes one event
// per read. The cases below feed the reader one byte at a time and three events in
// one chunk, and demand identical output — that is the property a buffered parser
// cannot satisfy on a live stream.

import { EventEmitter } from 'node:events';
import { PassThrough } from 'node:stream';
import { mkdtempSync, readFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { afterEach, describe, expect, it } from 'vitest';

import { SseReader, framesFromBody, jsonFrames } from './live-sse-reader.mjs';
import {
  LEGACY_ONLY_PROTOCOL_VERSIONS,
  NATIVE_PROTOCOL_VERSIONS,
  NativeDriver,
  PROTOCOL_HEADER,
  SESSION_HEADER,
  TOKEN_HEADER,
} from './live-driver-native.mjs';
import { FrameDecoder, StdioDriver, encodeFrame } from './live-driver-stdio.mjs';
import {
  REDACTED,
  ResourceLedger,
  readCapabilityToken,
  redact,
  secretValues,
  writeRedactedEvidence,
} from './live-resource-ledger.mjs';

const TEMP_DIRS: string[] = [];
afterEach(() => {
  while (TEMP_DIRS.length > 0) {
    const dir = TEMP_DIRS.pop();
    if (dir !== undefined) rmSync(dir, { recursive: true, force: true });
  }
});

function tempDir(): string {
  const dir = mkdtempSync(join(tmpdir(), 'task49-'));
  TEMP_DIRS.push(dir);
  return dir;
}

// ─────────────────────────────── SSE fragmentation ───────────────────────────

const SSE_STREAM =
  'event: message\ndata: {"jsonrpc":"2.0","method":"notifications/progress","params":{"progressToken":"t","progress":1}}\n\n' +
  ': keep-alive\n\n' +
  'id: 7\ndata: {"jsonrpc":"2.0","id":3,"result":{"ok":true}}\n\n';

// GOLDEN, not self-comparison. Comparing "byte at a time" against "one read"
// only proves the parser is CONSISTENT, and a parser that is uniformly wrong is
// perfectly consistent — a mutation that dispatched on every `data:` line passed
// both framing tests. The expected events are therefore written out by hand.
const SSE_GOLDEN = [
  {
    event: 'message',
    data: '{"jsonrpc":"2.0","method":"notifications/progress","params":{"progressToken":"t","progress":1}}',
    id: null,
    retry: null,
  },
  {
    event: null,
    data: '{"jsonrpc":"2.0","id":3,"result":{"ok":true}}',
    id: '7',
    retry: null,
  },
];

function readAll(chunks: readonly string[]) {
  const reader = new SseReader();
  const events = [];
  for (const chunk of chunks) events.push(...reader.push(chunk));
  events.push(...reader.end());
  return events;
}

describe('SSE reader — fragmentation is explicitly in scope', () => {
  it('parses a stream delivered as ONE read', () => {
    expect(readAll([SSE_STREAM])).toEqual(SSE_GOLDEN);
  });

  it('parses the stream delivered ONE BYTE AT A TIME into the same golden events', () => {
    expect(readAll([...SSE_STREAM])).toEqual(SSE_GOLDEN);
  });

  it('parses the stream split at EVERY possible boundary into the same golden events', () => {
    for (let cut = 1; cut < SSE_STREAM.length; cut += 1) {
      expect(readAll([SSE_STREAM.slice(0, cut), SSE_STREAM.slice(cut)]), `split at ${cut}`).toEqual(SSE_GOLDEN);
    }
  });

  it('dispatches on the blank line, never on each data line', () => {
    // Two `data:` lines belonging to ONE event must produce ONE event.
    const reader = new SseReader();
    const events = reader.push('data: a\ndata: b\n\n');
    expect(events).toHaveLength(1);
    expect(reader.dispatched).toBe(1);
    expect(events[0]?.data).toBe('a\nb');
  });

  it('returns THREE events from a single read containing three', () => {
    const stream = ['data: {"n":1}', '', 'data: {"n":2}', '', 'data: {"n":3}', '', ''].join('\n');
    const reader = new SseReader();
    const events = reader.push(stream);
    expect(events).toHaveLength(3);
    expect(jsonFrames(events)).toEqual([{ n: 1 }, { n: 2 }, { n: 3 }]);
  });

  it('parses a CRLF stream split at EVERY boundary into the same golden events', () => {
    // The CRLF sweep is what actually pins the partial-CR hold: an LF-only stream
    // never presents a CR that could still become a CRLF, so deleting the hold
    // passed the LF sweep untouched.
    const crlf = SSE_STREAM.split('\n').join('\r\n');
    for (let cut = 1; cut < crlf.length; cut += 1) {
      expect(readAll([crlf.slice(0, cut), crlf.slice(cut)]), `CRLF split at ${cut}`).toEqual(SSE_GOLDEN);
    }
  });

  it('holds a trailing CR back rather than dispatching an event one read early', () => {
    // The discriminating split is BETWEEN the CR and LF of the blank line that
    // terminates the event — not after the data line, which is already complete.
    const reader = new SseReader();
    expect(reader.push('data: {"a":1}\r\n\r')).toHaveLength(0);
    const events = reader.push('\n');
    expect(events).toHaveLength(1);
    expect(events[0]?.data).toBe('{"a":1}');
  });

  it('joins multi-line data with a newline, as the grammar requires', () => {
    const events = readAll(['data: {"a":\ndata: 1}\n\n']);
    expect(events[0]?.data).toBe('{"a":\n1}');
    expect(jsonFrames(events)).toEqual([{ a: 1 }]);
  });

  it('ignores comment keep-alives and unknown fields', () => {
    const events = readAll([': ping\nfoo: bar\ndata: {"a":1}\n\n']);
    expect(events).toHaveLength(1);
    expect(events[0]?.data).toBe('{"a":1}');
  });

  it('flushes an event the server never terminated with a blank line', () => {
    const reader = new SseReader();
    expect(reader.push('data: {"a":1}')).toHaveLength(0);
    const tail = reader.end();
    expect(jsonFrames(tail)).toEqual([{ a: 1 }]);
  });

  it('reads a plain JSON body that is not SSE at all', () => {
    expect(framesFromBody('{"jsonrpc":"2.0","id":1,"result":{}}')).toHaveLength(1);
  });

  it('survives a multi-byte character split across a chunk boundary', () => {
    const payload = Buffer.from('data: {"m":"日本語"}\n\n', 'utf8');
    const reader = new SseReader();
    const events = [];
    for (const byte of payload) events.push(...reader.push(Buffer.from([byte])));
    events.push(...reader.end());
    expect(jsonFrames(events)).toEqual([{ m: '日本語' }]);
  });
});

// ─────────────────────────── stdio framing and lifecycle ─────────────────────

describe('stdio driver — newline-delimited JSON framing', () => {
  it('encodes exactly one trailing newline', () => {
    expect(encodeFrame({ jsonrpc: '2.0', id: 1 })).toBe('{"jsonrpc":"2.0","id":1}\n');
  });

  it('reassembles a frame split across reads', () => {
    const decoder = new FrameDecoder();
    expect(decoder.push('{"jsonrpc":"2.0",')).toHaveLength(0);
    expect(decoder.push('"id":1,"result":{}}')).toHaveLength(0);
    expect(decoder.push('\n')).toEqual([{ jsonrpc: '2.0', id: 1, result: {} }]);
  });

  it('returns every frame from a read that carried several', () => {
    const decoder = new FrameDecoder();
    expect(decoder.push('{"id":1}\n{"id":2}\n{"id":3}\n')).toHaveLength(3);
  });

  it('ignores blank lines and counts (never throws on) a non-JSON line', () => {
    const decoder = new FrameDecoder();
    expect(decoder.push('\n\nnot json\n{"id":1}\n')).toEqual([{ id: 1 }]);
    expect(decoder.malformed).toBe(1);
  });

  it('parses byte-at-a-time identically to wholesale', () => {
    const text = '{"id":1}\n{"id":2}\n';
    const wholesale = new FrameDecoder().push(text);
    const bytewise = new FrameDecoder();
    const frames = [];
    for (const character of text) frames.push(...bytewise.push(character));
    expect(frames).toEqual(wholesale);
  });
});

class FakeChild extends EventEmitter {
  stdin = new PassThrough();
  stdout = new PassThrough();
  stderr = new PassThrough();
  pid = 4242;
  exitCode: number | null = null;
  signalCode: NodeJS.Signals | null = null;
  killed: NodeJS.Signals[] = [];
  ignoreSigterm = false;

  written(): string[] {
    return this.stdin.read()?.toString().split('\n').filter(Boolean) ?? [];
  }

  kill(signal: NodeJS.Signals) {
    this.killed.push(signal);
    if (signal === 'SIGTERM' && this.ignoreSigterm) return true;
    this.exitCode = signal === 'SIGKILL' ? null : 0;
    this.signalCode = signal;
    setImmediate(() => this.emit('exit', this.exitCode, signal));
    return true;
  }
}

function stdioHarness(env: Record<string, string> = {}) {
  const child = new FakeChild();
  const driver = new StdioDriver({
    cwd: tempDir(),
    env: { ...env },
    skipFreshnessCheck: true,
    spawnFn: (() => child) as never,
  });
  return { child, driver };
}

/** Read every JSON frame the driver wrote to the child's stdin. */
function sent(child: FakeChild): Record<string, unknown>[] {
  const raw = child.stdin.read();
  if (raw === null) return [];
  return String(raw).split('\n').filter((line) => line.trim().length > 0).map((line) => JSON.parse(line));
}

describe('stdio driver — handshake, correlation, auth', () => {
  it('sends initialize then notifications/initialized and correlates by id', async () => {
    const { child, driver } = stdioHarness();
    const starting = driver.start({ timeoutMs: 2000 });
    // The handshake response arrives split across two reads, as a real pipe would.
    await new Promise((settle) => setImmediate(settle));
    child.stdout.write('{"jsonrpc":"2.0","id":1001,"result":{"protocolVersion":');
    child.stdout.write('"2025-06-18","capabilities":{}}}\n');
    const outcome = await starting;
    expect(outcome.ok).toBe(true);
    expect(outcome.negotiatedVersion).toBe('2025-06-18');

    const frames = sent(child);
    expect(frames[0]?.method).toBe('initialize');
    expect(frames[1]?.method).toBe('notifications/initialized');
    expect(frames[1]).not.toHaveProperty('id');
  });

  it('passes the capability token to the child as MCP_AUTOMATION_CAPABILITY_TOKEN', () => {
    const { driver } = stdioHarness({ MCP_QA_TOKEN: 'a-loopback-secret' });
    expect(driver.token).toBe('a-loopback-secret');
    expect(driver.tokenSource).toBe('MCP_QA_TOKEN');
  });

  it('presents no token when the environment sets none, rather than inventing one', () => {
    const { driver } = stdioHarness();
    expect(driver.token).toBeNull();
    expect(driver.tokenSource).toBe('unset');
  });

  it('routes a server notification away from the pending-response map', async () => {
    const { child, driver } = stdioHarness();
    const starting = driver.start({ timeoutMs: 2000 });
    await new Promise((settle) => setImmediate(settle));
    child.stdout.write('{"jsonrpc":"2.0","method":"notifications/progress","params":{"progressToken":"t"}}\n');
    child.stdout.write('{"jsonrpc":"2.0","id":1001,"result":{}}\n');
    await starting;
    expect(driver.notifications).toHaveLength(1);
    expect(driver.notifications[0]?.method).toBe('notifications/progress');
  });

  it('returns a timeout rather than hanging when no response arrives', async () => {
    const { driver } = stdioHarness();
    const outcome = await driver.start({ timeoutMs: 40 });
    expect(outcome.ok).toBe(false);
    expect(outcome.reason).toBe('INITIALIZE_TIMEOUT');
  });
});

describe('stdio driver — cancellation and shutdown', () => {
  it('writes notifications/cancelled with the requestId and reason, and no id', async () => {
    const { child, driver } = stdioHarness();
    const starting = driver.start({ timeoutMs: 2000 });
    await new Promise((settle) => setImmediate(settle));
    child.stdout.write('{"jsonrpc":"2.0","id":1001,"result":{}}\n');
    await starting;
    sent(child);
    await driver.cancel(1002, 'corpus cancellation probe');
    const frames = sent(child);
    expect(frames[0]?.method).toBe('notifications/cancelled');
    expect(frames[0]?.params).toEqual({ requestId: 1002, reason: 'corpus cancellation probe' });
    expect(frames[0]).not.toHaveProperty('id');
  });

  it('SIGTERMs the child and WAITS for it to be reaped', async () => {
    const { child, driver } = stdioHarness();
    const starting = driver.start({ timeoutMs: 2000 });
    await new Promise((settle) => setImmediate(settle));
    child.stdout.write('{"jsonrpc":"2.0","id":1001,"result":{}}\n');
    await starting;
    const closed = await driver.close({ graceMs: 500 });
    expect(closed.stopped).toBe(true);
    expect(child.killed).toEqual(['SIGTERM']);
    expect(driver.verifyChildReleased().released).toBe(true);
  });

  it('escalates to SIGKILL when the child ignores SIGTERM', async () => {
    const { child, driver } = stdioHarness();
    child.ignoreSigterm = true;
    const starting = driver.start({ timeoutMs: 2000 });
    await new Promise((settle) => setImmediate(settle));
    child.stdout.write('{"jsonrpc":"2.0","id":1001,"result":{}}\n');
    await starting;
    const closed = await driver.close({ graceMs: 30 });
    expect(closed.escalated).toBe(true);
    expect(child.killed).toEqual(['SIGTERM', 'SIGKILL']);
  });

  it('is idempotent, so teardown from a finally block cannot double-kill', async () => {
    const { child, driver } = stdioHarness();
    const starting = driver.start({ timeoutMs: 2000 });
    await new Promise((settle) => setImmediate(settle));
    child.stdout.write('{"jsonrpc":"2.0","id":1001,"result":{}}\n');
    await starting;
    await driver.close({ graceMs: 200 });
    const second = await driver.close({ graceMs: 200 });
    expect(second.alreadyClosed).toBe(true);
    expect(child.killed).toEqual(['SIGTERM']);
  });

  it('reports an unreaped child HONESTLY rather than assuming close() worked', () => {
    const { child, driver } = stdioHarness();
    driver.child = child as never;
    const verdict = driver.verifyChildReleased();
    expect(verdict.released).toBe(false);
    expect(verdict.observed).toContain('STILL RUNNING');
  });
});

// ───────────────────────────── native driver ─────────────────────────────────

type Captured = {
  method: string;
  path: string;
  headers: Record<string, string>;
  body: string | null;
};

function nativeHarness(options: {
  env?: Record<string, string>;
  reply?: (captured: Captured, index: number) => { status?: number; headers?: Record<string, string>; body?: string };
} = {}) {
  const captured: Captured[] = [];
  const requestFn = (async (spec: Captured & { timeoutMs: number; onFrame?: (frame: unknown) => void }) => {
    captured.push({ method: spec.method, path: spec.path, headers: spec.headers, body: spec.body });
    const canned = options.reply?.({ method: spec.method, path: spec.path, headers: spec.headers, body: spec.body }, captured.length - 1) ?? {};
    const body = canned.body ?? '';
    const reader = new SseReader();
    const frames = jsonFrames([...reader.push(body), ...reader.end()]);
    const all = frames.length > 0 ? frames : framesFromBody(body);
    for (const frame of all) spec.onFrame?.(frame);
    return {
      status: canned.status ?? 200,
      headers: canned.headers ?? {},
      body,
      frames: all,
      notifications: [],
      ms: 1,
    };
  }) as never;
  const driver = new NativeDriver({ env: { ...options.env }, requestFn });
  return { captured, driver };
}

const INIT_BODY = (version: string) =>
  `{"jsonrpc":"2.0","id":1,"result":{"protocolVersion":"${version}","capabilities":{},"serverInfo":{"name":"x","version":"1"}}}`;

describe('native driver — initialize, session and protocol versions', () => {
  it('POSTs initialize with no session and no MCP-Protocol-Version header', async () => {
    const { captured, driver } = nativeHarness({
      reply: () => ({ headers: { 'mcp-session-id': 'sess-1' }, body: INIT_BODY('2025-06-18') }),
    });
    const outcome = await driver.initialize();
    expect(outcome.ok).toBe(true);
    expect(outcome.sessionId).toBe('sess-1');
    expect(outcome.negotiatedVersion).toBe('2025-06-18');
    expect(captured[0]?.method).toBe('POST');
    expect(captured[0]?.path).toBe('/mcp');
    expect(captured[0]?.headers[SESSION_HEADER]).toBeUndefined();
    expect(captured[0]?.headers[PROTOCOL_HEADER]).toBeUndefined();
    expect(JSON.parse(String(captured[0]?.body)).params.protocolVersion).toBe('2025-06-18');
  });

  it('carries session and negotiated protocol version on every post-initialize request', async () => {
    const { captured, driver } = nativeHarness({
      reply: (_c, index) => (index === 0
        ? { headers: { 'mcp-session-id': 'sess-2' }, body: INIT_BODY('2025-11-25') }
        : { body: '{"jsonrpc":"2.0","id":1001,"result":{}}' }),
    });
    await driver.initialize();
    await driver.callTool({ operation: 'search', query: 'x' });
    const call = captured[1];
    expect(call?.headers[SESSION_HEADER]).toBe('sess-2');
    expect(call?.headers[PROTOCOL_HEADER]).toBe('2025-11-25');
  });

  it('records the version the server negotiated, not the one the client asked for', async () => {
    const { driver } = nativeHarness({
      reply: () => ({ headers: { 'mcp-session-id': 's' }, body: INIT_BODY('2025-03-26') }),
    });
    const outcome = await driver.initialize({ protocolVersion: '2025-11-25' });
    expect(outcome.requestedVersion).toBe('2025-11-25');
    expect(outcome.negotiatedVersion).toBe('2025-03-26');
  });

  it('pins the three modern versions the native transport supports and excludes the legacy pair', () => {
    expect([...NATIVE_PROTOCOL_VERSIONS]).toEqual(['2025-11-25', '2025-06-18', '2025-03-26']);
    for (const legacy of LEGACY_ONLY_PROTOCOL_VERSIONS) {
      expect(NATIVE_PROTOCOL_VERSIONS).not.toContain(legacy);
    }
    // The 2026-07-28 RC is deliberately unimplemented; a driver must never claim it.
    expect(NATIVE_PROTOCOL_VERSIONS).not.toContain('2026-07-28');
  });

  it('matches the version list the plugin actually compiles', () => {
    const source = readFileSync(
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Transport/McpNativeTransportPrivate.h',
      'utf8',
    );
    for (const version of NATIVE_PROTOCOL_VERSIONS) {
      expect(source, `plugin does not list ${version}`).toContain(`TEXT("${version}")`);
    }
    for (const legacy of LEGACY_ONLY_PROTOCOL_VERSIONS) {
      expect(source, `plugin unexpectedly lists ${legacy}`).not.toContain(`TEXT("${legacy}")`);
    }
  });
});

describe('native driver — auth', () => {
  it('attaches the capability token header when the environment provides one', async () => {
    const { captured, driver } = nativeHarness({
      env: { MCP_QA_TOKEN: 'loopback-secret-value' },
      reply: () => ({ headers: { 'mcp-session-id': 's' }, body: INIT_BODY('2025-06-18') }),
    });
    await driver.initialize();
    expect(captured[0]?.headers[TOKEN_HEADER]).toBe('loopback-secret-value');
  });

  it('omits the header entirely rather than sending an empty or invented token', async () => {
    const { captured, driver } = nativeHarness({
      reply: () => ({ headers: { 'mcp-session-id': 's' }, body: INIT_BODY('2025-06-18') }),
    });
    await driver.initialize();
    expect(captured[0]?.headers).not.toHaveProperty(TOKEN_HEADER);
  });

  it('resolves the token from the documented env vars in order', () => {
    expect(readCapabilityToken({ MCP_QA_TOKEN: 'a', MCP_AUTOMATION_CAPABILITY_TOKEN: 'b' }).token).toBe('a');
    expect(readCapabilityToken({ MCP_AUTOMATION_CAPABILITY_TOKEN: 'b' }).source).toBe('MCP_AUTOMATION_CAPABILITY_TOKEN');
    expect(readCapabilityToken({}).token).toBeNull();
  });
});

describe('native driver — GET SSE stream, cancellation and shutdown', () => {
  it('opens GET /mcp with Accept: text/event-stream and the session header', async () => {
    const { captured, driver } = nativeHarness({
      reply: (_c, index) => (index === 0
        ? { headers: { 'mcp-session-id': 'sess-3' }, body: INIT_BODY('2025-06-18') }
        : { status: 200, body: '' }),
    });
    await driver.initialize();
    await driver.openNotificationStream({ timeoutMs: 50 });
    const get = captured.find((entry) => entry.method === 'GET');
    expect(get).toBeDefined();
    expect(get?.headers.Accept).toBe('text/event-stream');
    expect(get?.headers[SESSION_HEADER]).toBe('sess-3');
    expect(get?.body).toBeNull();
  });

  it('refuses to open a notification stream before a session exists', async () => {
    const { driver } = nativeHarness();
    expect((await driver.openNotificationStream()).reason).toBe('NO_SESSION');
  });

  it('collects progress notifications arriving alongside the response on one POST stream', async () => {
    const streamed =
      'data: {"jsonrpc":"2.0","method":"notifications/progress","params":{"progressToken":"tok","progress":1}}\n\n' +
      'data: {"jsonrpc":"2.0","id":1001,"result":{"content":[]}}\n\n';
    const { driver } = nativeHarness({
      reply: (_c, index) => (index === 0
        ? { headers: { 'mcp-session-id': 's' }, body: INIT_BODY('2025-06-18') }
        : { body: streamed }),
    });
    await driver.initialize();
    const call = await driver.callTool({ operation: 'search', query: 'x' });
    expect(call.response?.id).toBe(call.requestId);
    expect(call.streamNotifications).toHaveLength(1);
    expect(driver.notifications).toHaveLength(1);
  });

  it('POSTs notifications/cancelled with requestId and reason and no id', async () => {
    const { captured, driver } = nativeHarness({
      reply: (_c, index) => (index === 0 ? { headers: { 'mcp-session-id': 's' }, body: INIT_BODY('2025-06-18') } : {}),
    });
    await driver.initialize();
    await driver.cancel(77, 'corpus cancellation probe');
    const frame = JSON.parse(String(captured[1]?.body));
    expect(frame.method).toBe('notifications/cancelled');
    expect(frame.params).toEqual({ requestId: 77, reason: 'corpus cancellation probe' });
    expect(frame).not.toHaveProperty('id');
  });

  it('DELETEs the session on close and is idempotent', async () => {
    const { captured, driver } = nativeHarness({
      reply: (_c, index) => (index === 0 ? { headers: { 'mcp-session-id': 'sess-4' }, body: INIT_BODY('2025-06-18') } : {}),
    });
    await driver.initialize();
    const first = await driver.close();
    expect(first.deleted).toBe(true);
    const remove = captured.find((entry) => entry.method === 'DELETE');
    expect(remove?.headers[SESSION_HEADER]).toBe('sess-4');
    const second = await driver.close();
    expect(second.alreadyClosed).toBe(true);
    expect(captured.filter((entry) => entry.method === 'DELETE')).toHaveLength(1);
  });

  it('verifies the session is really gone instead of trusting the DELETE status', async () => {
    const { driver } = nativeHarness({
      reply: (captured, index) => {
        if (index === 0) return { headers: { 'mcp-session-id': 's' }, body: INIT_BODY('2025-06-18') };
        if (captured.method === 'DELETE') return { status: 200 };
        return { status: 404, body: 'Session not found' };
      },
    });
    await driver.initialize();
    await driver.close();
    const verdict = await driver.verifySessionReleased();
    expect(verdict.released).toBe(true);
    expect(verdict.observed).toContain('404');
  });

  it('reports a session that SURVIVED the DELETE as leaked', async () => {
    const { driver } = nativeHarness({
      reply: (_c, index) => (index === 0 ? { headers: { 'mcp-session-id': 's' }, body: INIT_BODY('2025-06-18') } : { status: 200 }),
    });
    await driver.initialize();
    await driver.close();
    expect((await driver.verifySessionReleased()).released).toBe(false);
  });
});

// ───────────────────────── redaction and the resource ledger ─────────────────

describe('redaction — a token must never reach a log, receipt or evidence file', () => {
  const env = { MCP_QA_TOKEN: 'super-secret-loopback-token' };

  it('masks the token deep inside nested structures', () => {
    const report = {
      headers: { 'X-MCP-Capability-Token': 'super-secret-loopback-token' },
      rows: [{ message: 'auth failed for super-secret-loopback-token, retrying' }],
    };
    const clean = JSON.stringify(redact(report, secretValues(env)));
    expect(clean).not.toContain('super-secret-loopback-token');
    expect(clean).toContain(REDACTED);
  });

  it('masks a token used as an object KEY', () => {
    const clean = redact({ 'super-secret-loopback-token': 1 }, secretValues(env));
    expect(Object.keys(clean as Record<string, unknown>)).toEqual([REDACTED]);
  });

  it('ignores values too short to be a credential, so evidence is not shredded', () => {
    expect(secretValues({ MCP_QA_TOKEN: 'ab' })).toEqual([]);
  });

  it('writes an evidence file that does not contain the token', () => {
    const path = join(tempDir(), 'evidence.json');
    writeRedactedEvidence(path, { token: 'super-secret-loopback-token', nested: ['super-secret-loopback-token'] }, env);
    const written = readFileSync(path, 'utf8');
    expect(written).not.toContain('super-secret-loopback-token');
    expect(written).toContain(REDACTED);
  });
});

describe('resource ledger — cleanup is an acceptance criterion', () => {
  it('emits a receipt per resource and counts a leak when verification says so', async () => {
    const ledger = new ResourceLedger();
    ledger.register('process', 'pid-1', { pid: 1 }, async () => {}, async () => ({ released: true, observed: 'pgrep count 0' }));
    ledger.register('session', 'sess-1', {}, async () => {}, async () => ({ released: false, observed: 'session still answers' }));
    const outcome = await ledger.teardown();
    expect(outcome.total).toBe(2);
    expect(outcome.released).toBe(1);
    expect(outcome.leaked).toBe(1);
    expect(outcome.receipts.map((receipt) => receipt.id)).toEqual(['sess-1', 'pid-1']);
  });

  it('still verifies a resource whose release THREW, rather than skipping it', async () => {
    const ledger = new ResourceLedger();
    ledger.register('process', 'pid-2', {}, async () => { throw new Error('kill failed'); },
      async () => ({ released: false, observed: 'still running' }));
    const outcome = await ledger.teardown();
    expect(outcome.receipts[0]?.releaseError).toBe('kill failed');
    expect(outcome.leaked).toBe(1);
  });

  it('tears down in reverse order of registration', async () => {
    const order: string[] = [];
    const ledger = new ResourceLedger();
    for (const id of ['a', 'b', 'c']) {
      ledger.register('tempdir', id, {}, async () => { order.push(id); }, async () => ({ released: true, observed: 'removed' }));
    }
    await ledger.teardown();
    expect(order).toEqual(['c', 'b', 'a']);
  });

  it('refuses an unknown resource kind rather than silently not tracking it', () => {
    expect(() => new ResourceLedger().register('mystery', 'x', {}, async () => {}, async () => ({ released: true, observed: '' })))
      .toThrow(/unknown resource kind/u);
  });
});
