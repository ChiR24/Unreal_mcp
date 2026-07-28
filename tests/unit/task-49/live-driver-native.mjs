// @ts-check
// tests/unit/task-49/live-driver-native.mjs
// Task 49 — the REAL native `/mcp` driver: initialize / GET SSE / POST / DELETE.
//
// This replaces the ad-hoc native halves of task40/41/42/43/46. Those all worked,
// and all shared two shortcuts this driver does not take:
//
//   1. They buffered the whole HTTP body and THEN split it on "data: ". That
//      assumes one read per event. This driver feeds every chunk into an
//      incremental SseReader, so a frame split across TCP reads — or three
//      frames arriving in one — is parsed correctly (see live-sse-reader.mjs).
//   2. None of them opened the persistent `GET /mcp` notification stream, so
//      server-initiated notifications were invisible. Progress and cancellation
//      cannot be honestly observed without it.
//
// Every HTTP call goes through an injectable `requestFn`, which is what makes the
// offline driver tests real tests: framing, auth, protocol-version negotiation,
// SSE fragmentation, cancellation and shutdown are all exercised against a fake
// transport that records exactly what went on the wire, with no editor present.
//
// The capability token is read from the environment (never a literal here) and is
// attached as `X-MCP-Capability-Token`. It is never logged and never written to a
// report — writeRedactedEvidence in live-resource-ledger.mjs is the only writer.

import http from 'node:http';

import { SseReader, framesFromBody } from './live-sse-reader.mjs';
import { readCapabilityToken } from './live-resource-ledger.mjs';

/** The three modern versions the native transport supports, newest first.
 * It deliberately does NOT implement the later 2026-07-28 RC, and it does NOT
 * accept the two legacy versions the TypeScript SDK still takes. Pinned here so a
 * driver run that negotiates something else is a finding, not a surprise. */
export const NATIVE_PROTOCOL_VERSIONS = Object.freeze(['2025-11-25', '2025-06-18', '2025-03-26']);

/** Versions the TS stdio surface accepts but native must REFUSE. */
export const LEGACY_ONLY_PROTOCOL_VERSIONS = Object.freeze(['2024-11-05', '2024-10-07']);

export const SESSION_HEADER = 'Mcp-Session-Id';
export const TOKEN_HEADER = 'X-MCP-Capability-Token';
export const PROTOCOL_HEADER = 'MCP-Protocol-Version';

/**
 * @typedef {{ status: number, headers: Record<string, string|string[]|undefined>,
 *   body: string, frames: unknown[], notifications: unknown[], ms: number }} NativeResponse
 */

/**
 * Default transport: a real HTTP request that streams its body through an
 * incremental SSE reader.
 * @param {{ host: string, port: number, method: string, path: string,
 *   headers: Record<string, string>, body: string|null, timeoutMs: number,
 *   onFrame?: (frame: unknown) => void }} spec
 * @returns {Promise<NativeResponse>}
 */
export function nodeHttpRequest(spec) {
  const startedAt = Date.now();
  return new Promise((settle) => {
    const reader = new SseReader();
    /** @type {unknown[]} */
    const frames = [];
    let raw = '';
    const request = http.request({
      host: spec.host, port: spec.port, path: spec.path, method: spec.method, headers: spec.headers,
    }, (response) => {
      response.on('data', (chunk) => {
        raw += String(chunk);
        // Incremental: a chunk boundary anywhere is fine, including mid-frame.
        for (const event of reader.push(chunk)) {
          if (event.data.length === 0) continue;
          try {
            const frame = JSON.parse(event.data);
            frames.push(frame);
            spec.onFrame?.(frame);
          } catch { /* not a JSON data frame */ }
        }
      });
      response.on('end', () => {
        for (const event of reader.end()) {
          if (event.data.length === 0) continue;
          try {
            const frame = JSON.parse(event.data);
            frames.push(frame);
            spec.onFrame?.(frame);
          } catch { /* not a JSON data frame */ }
        }
        // A plain `application/json` reply produces no SSE events at all.
        const all = frames.length > 0 ? frames : framesFromBody(raw);
        settle({
          status: response.statusCode ?? 0,
          headers: /** @type {Record<string, string|string[]|undefined>} */ (response.headers),
          body: raw, frames: all, notifications: [], ms: Date.now() - startedAt,
        });
      });
    });
    request.setTimeout(spec.timeoutMs, () => {
      request.destroy();
      settle({ status: 0, headers: {}, body: raw, frames, notifications: [], ms: Date.now() - startedAt });
    });
    request.on('error', (error) => settle({
      status: -1, headers: {}, body: String(error), frames, notifications: [], ms: Date.now() - startedAt,
    }));
    request.end(spec.body === null ? undefined : spec.body);
  });
}

/** Split a batch of frames into the response for `id` and everything else.
 * @param {readonly unknown[]} frames @param {number} id */
function partitionFrames(frames, id) {
  /** @type {any} */
  let response = null;
  /** @type {unknown[]} */
  const notifications = [];
  for (const frame of frames) {
    const candidate = /** @type {any} */ (frame);
    if (candidate && candidate.id === id && (candidate.result !== undefined || candidate.error !== undefined)) {
      response = candidate;
    } else {
      notifications.push(candidate);
    }
  }
  return { response, notifications };
}

export class NativeDriver {
  /**
   * @param {{ host?: string, port?: number, protocolVersion?: string, clientName?: string,
   *   env?: NodeJS.ProcessEnv, requestFn?: typeof nodeHttpRequest }} [options]
   */
  constructor(options = {}) {
    this.name = 'native-http-sse';
    this.host = options.host ?? '127.0.0.1';
    this.port = options.port ?? Number(options.env?.MCP_QA_NATIVE_PORT ?? process.env.MCP_QA_NATIVE_PORT ?? 3000);
    this.requestedVersion = options.protocolVersion ?? NATIVE_PROTOCOL_VERSIONS[1];
    this.clientName = options.clientName ?? 'task49-live-corpus';
    const credentials = readCapabilityToken(options.env ?? process.env);
    /** @type {string|null} */
    this.token = credentials.token;
    this.tokenSource = credentials.source;
    this.requestFn = options.requestFn ?? nodeHttpRequest;
    /** @type {string|null} */
    this.sessionId = null;
    /** @type {string|null} */
    this.negotiatedVersion = null;
    this.rpcId = 1000;
    /** Server-initiated frames observed on ANY stream, in arrival order. */
    this.notifications = /** @type {any[]} */ ([]);
    /** @type {{ close: () => void }|null} */
    this.notificationStream = null;
    this.closed = false;
  }

  /** Headers for a post-initialize request. `initialize` itself is exempt from the version header. */
  headers(extra = {}, { includeSession = true, includeVersion = true } = {}) {
    /** @type {Record<string, string>} */
    const headers = {
      'Content-Type': 'application/json',
      Accept: 'application/json, text/event-stream',
    };
    if (this.token !== null) headers[TOKEN_HEADER] = this.token;
    if (includeSession && this.sessionId !== null) headers[SESSION_HEADER] = this.sessionId;
    if (includeVersion && this.negotiatedVersion !== null) headers[PROTOCOL_HEADER] = this.negotiatedVersion;
    return { ...headers, ...extra };
  }

  /**
   * @param {string} method @param {string|null} bodyText
   * @param {Record<string, string>} headers @param {number} timeoutMs
   * @param {(frame: unknown) => void} [onFrame]
   */
  async send(method, bodyText, headers, timeoutMs, onFrame) {
    const full = bodyText === null ? headers : { ...headers, 'Content-Length': String(Buffer.byteLength(bodyText)) };
    return this.requestFn({
      host: this.host, port: this.port, method, path: '/mcp',
      headers: full, body: bodyText, timeoutMs, onFrame,
    });
  }

  /**
   * POST initialize. Returns the negotiated version and session id, or a typed
   * failure — a driver that cannot say WHY it failed to connect turns every
   * downstream case into an indistinguishable red.
   * @param {{ timeoutMs?: number, protocolVersion?: string }} [options]
   */
  async initialize(options = {}) {
    const version = options.protocolVersion ?? this.requestedVersion;
    const body = JSON.stringify({
      jsonrpc: '2.0', id: 1, method: 'initialize',
      params: {
        protocolVersion: version,
        capabilities: {},
        clientInfo: { name: this.clientName, version: '1.0.0' },
      },
    });
    const response = await this.send('POST', body, this.headers({}, { includeSession: false, includeVersion: false }), options.timeoutMs ?? 30_000);
    const header = response.headers?.['mcp-session-id'];
    this.sessionId = typeof header === 'string' ? header : (Array.isArray(header) ? String(header[0]) : null);
    const first = /** @type {any} */ (response.frames[0]);
    this.negotiatedVersion = typeof first?.result?.protocolVersion === 'string' ? first.result.protocolVersion : null;
    return {
      ok: this.sessionId !== null && response.status === 200,
      status: response.status,
      sessionId: this.sessionId,
      requestedVersion: version,
      negotiatedVersion: this.negotiatedVersion,
      tokenPresented: this.token !== null,
      tokenSource: this.tokenSource,
      body: response.body,
    };
  }

  /** The `notifications/initialized` handshake completion. Fire-and-forget by spec. */
  async notifyInitialized(timeoutMs = 15_000) {
    const body = JSON.stringify({ jsonrpc: '2.0', method: 'notifications/initialized' });
    return this.send('POST', body, this.headers(), timeoutMs);
  }

  /**
   * Open the persistent `GET /mcp` SSE notification stream. The socket is PARKED
   * by the server, so this resolves as soon as the request is in flight and keeps
   * appending to `this.notifications` until close(). Every frame arrives through
   * the incremental reader, so a notification split across reads is not lost.
   * @param {{ timeoutMs?: number }} [options]
   */
  async openNotificationStream(options = {}) {
    if (this.sessionId === null) return { ok: false, reason: 'NO_SESSION' };
    const headers = this.headers({ Accept: 'text/event-stream' });
    let settled = false;
    /** @type {Promise<NativeResponse>} */
    const inFlight = this.send('GET', null, headers, options.timeoutMs ?? 3_600_000, (frame) => {
      this.notifications.push(frame);
    });
    // Give the server a beat to reject an unacceptable Accept/session/version.
    const outcome = await Promise.race([
      inFlight.then((response) => { settled = true; return response; }),
      new Promise((resolve) => { setTimeout(() => resolve(null), 750); }),
    ]);
    if (settled && outcome !== null) {
      const response = /** @type {NativeResponse} */ (outcome);
      // A parked stream does not end this fast; an immediate end is a refusal.
      return { ok: response.status === 200, reason: `CLOSED_${response.status}`, status: response.status, body: response.body };
    }
    this.notificationStream = { close: () => { /* released when the socket is torn down by close() */ } };
    this.streamPromise = inFlight;
    return { ok: true, reason: 'PARKED' };
  }

  /**
   * One `tools/call`. Returns the JSON-RPC response frame plus every notification
   * that arrived on the SAME stream, so progress is observable without guessing.
   * @param {Record<string, unknown>} args gateway arguments
   * @param {{ meta?: Record<string, unknown>, task?: Record<string, unknown>,
   *   timeoutMs?: number, toolName?: string, sessionId?: string|null }} [options]
   */
  async callTool(args, options = {}) {
    const id = ++this.rpcId;
    /** @type {Record<string, unknown>} */
    const params = { name: options.toolName ?? 'unreal', arguments: args };
    if (options.meta) params._meta = options.meta;
    if (options.task) params.task = options.task;
    const body = JSON.stringify({ jsonrpc: '2.0', id, method: 'tools/call', params });
    const headers = options.sessionId === undefined
      ? this.headers()
      : this.headers(options.sessionId === null ? {} : { [SESSION_HEADER]: options.sessionId }, { includeSession: false });
    const response = await this.send('POST', body, headers, options.timeoutMs ?? 120_000, (frame) => {
      const candidate = /** @type {any} */ (frame);
      if (candidate?.method !== undefined) this.notifications.push(candidate);
    });
    const split = partitionFrames(response.frames, id);
    return {
      requestId: id,
      status: response.status,
      body: response.body,
      response: split.response,
      streamNotifications: split.notifications,
      frameCount: response.frames.length,
      ms: response.ms,
    };
  }

  /** Raw JSON-RPC for methods that are not tools/call (tasks/*, ping, ...).
   * @param {string} method @param {Record<string, unknown>} params
   * @param {{ timeoutMs?: number }} [options] */
  async request(method, params, options = {}) {
    const id = ++this.rpcId;
    const body = JSON.stringify({ jsonrpc: '2.0', id, method, params });
    const response = await this.send('POST', body, this.headers(), options.timeoutMs ?? 60_000);
    const split = partitionFrames(response.frames, id);
    return { requestId: id, status: response.status, body: response.body, response: split.response };
  }

  /**
   * `notifications/cancelled` for an in-flight request. Advisory by spec: the
   * server may already have dispatched the work, so the driver reports that it
   * SENT the cancellation, never that the work stopped. Claiming otherwise is how
   * a cancellation test passes against a server that ignores cancellation.
   * @param {number} requestId @param {string} reason
   */
  async cancel(requestId, reason) {
    const body = JSON.stringify({
      jsonrpc: '2.0', method: 'notifications/cancelled',
      params: { requestId, reason },
    });
    const response = await this.send('POST', body, this.headers(), 15_000);
    return { sent: true, status: response.status, requestId, reason };
  }

  /**
   * DELETE the session and drop the parked GET stream. Idempotent: teardown runs
   * from a finally block and must not throw a second time on a second call.
   */
  async close() {
    if (this.closed) return { deleted: false, alreadyClosed: true, status: null };
    this.closed = true;
    if (this.sessionId === null) return { deleted: false, alreadyClosed: false, status: null };
    const response = await this.send('DELETE', null, this.headers(), 15_000);
    const deleted = response.status === 200 || response.status === 204;
    this.notificationStream = null;
    return { deleted, alreadyClosed: false, status: response.status, sessionId: this.sessionId };
  }

  /** Independent check that the session is really gone: a call on it must be refused. */
  async verifySessionReleased() {
    if (this.sessionId === null) return { released: true, observed: 'no session was ever created' };
    const probe = await this.send('POST', JSON.stringify({
      jsonrpc: '2.0', id: ++this.rpcId, method: 'tools/list', params: {},
    }), this.headers({ [SESSION_HEADER]: this.sessionId }, { includeSession: false }), 10_000);
    // 404/400 means the session id no longer resolves. 200 means it still does.
    const released = probe.status !== 200;
    return { released, observed: `post-DELETE tools/list on the same session id answered HTTP ${probe.status}` };
  }
}
