// @ts-check
// tests/unit/task-49/live-driver-stdio.mjs
// Task 49 — the REAL built-CLI stdio driver: `node dist/cli.js` over
// newline-delimited JSON-RPC, reaching the editor through the WebSocket bridge.
//
// IT SPAWNS A BUILD, NOT THE SOURCE. Two of Task 46's four original "HIGH"
// divergences were stale-`dist/` artifacts that read as live defects and cost a
// full remediation cycle. So this driver calls assertDistFresh() from
// tests/unit/task-46/dist-freshness.mjs and REFUSES LOUDLY on a stale or missing
// build. It never rebuilds silently: an operator must never have to infer which
// bytes were measured.
//
// The bridge token comes from the environment as MCP_AUTOMATION_CAPABILITY_TOKEN.
// With bRequireCapabilityToken=True on the plugin, a driver that omits it answers
// NOT_CONNECTED for every case — which looks exactly like a broken server and is
// really a broken probe. Readiness is therefore polled explicitly, and a run that
// never connects is reported BLOCKED rather than scored.
//
// Framing is a state machine, not a split(): stdout chunk boundaries are arbitrary
// and a JSON frame can straddle them. `FrameDecoder` is exported so the offline
// tests can feed it byte-at-a-time.

import { spawn } from 'node:child_process';

import { assertDistFresh, BUILD_OUTPUT_ENTRY } from '../task-46/dist-freshness.mjs';
import { readCapabilityToken } from './live-resource-ledger.mjs';

/** Versions the TypeScript SDK accepts: the three modern ones plus two legacy. */
export const STDIO_PROTOCOL_VERSIONS = Object.freeze([
  '2025-11-25', '2025-06-18', '2025-03-26', '2024-11-05', '2024-10-07',
]);

/** @param {Record<string, unknown>} frame @returns {string} */
export function encodeFrame(frame) {
  return `${JSON.stringify(frame)}\n`;
}

/**
 * Newline-delimited JSON decoder that survives arbitrary chunk boundaries: a
 * frame split across two reads, several frames in one read, blank lines, and a
 * trailing partial frame that only completes on the next chunk.
 */
export class FrameDecoder {
  constructor() {
    this.buffer = '';
    this.malformed = 0;
  }

  /** @param {Buffer|string} chunk @returns {any[]} the frames completed by THIS chunk */
  push(chunk) {
    this.buffer += String(chunk);
    /** @type {any[]} */
    const frames = [];
    for (;;) {
      const index = this.buffer.indexOf('\n');
      if (index < 0) break;
      const line = this.buffer.slice(0, index).trim();
      this.buffer = this.buffer.slice(index + 1);
      if (line.length === 0) continue;
      try {
        frames.push(JSON.parse(line));
      } catch {
        // A non-JSON line on stdout is a protocol violation worth counting, not
        // worth throwing over: routeStdoutLogsToStderr() exists to prevent it,
        // and a count here is how we would notice it regressed.
        this.malformed += 1;
      }
    }
    return frames;
  }
}

export class StdioDriver {
  /**
   * @param {{ cwd?: string, env?: NodeJS.ProcessEnv, protocolVersion?: string,
   *   clientName?: string, spawnFn?: typeof spawn, skipFreshnessCheck?: boolean }} [options]
   */
  constructor(options = {}) {
    this.name = 'stdio-jsonrpc';
    this.cwd = options.cwd ?? process.cwd();
    this.baseEnv = options.env ?? process.env;
    this.requestedVersion = options.protocolVersion ?? '2025-06-18';
    this.clientName = options.clientName ?? 'task49-live-corpus';
    this.spawnFn = options.spawnFn ?? spawn;
    this.skipFreshnessCheck = options.skipFreshnessCheck === true;
    const credentials = readCapabilityToken(this.baseEnv);
    /** @type {string|null} */
    this.token = credentials.token;
    this.tokenSource = credentials.source;
    this.rpcId = 1000;
    this.decoder = new FrameDecoder();
    /** @type {Map<number, (frame: any) => void>} */
    this.pending = new Map();
    /** @type {any[]} */
    this.notifications = [];
    /** @type {import('node:child_process').ChildProcess|null} */
    this.child = null;
    /** @type {{ entry: string, builtAt: string|null, newestInput: string|null }|null} */
    this.buildUnderTest = null;
    this.stderrTail = '';
    this.closed = false;
    /** @type {string|null} */
    this.negotiatedVersion = null;
  }

  /**
   * Spawn the built CLI and complete the MCP handshake. Throws StaleBuildRefusal
   * BEFORE spawning anything if dist/ is behind src/.
   * @param {{ timeoutMs?: number }} [options]
   */
  async start(options = {}) {
    if (!this.skipFreshnessCheck) {
      const freshness = assertDistFresh(this.cwd);
      this.buildUnderTest = {
        entry: freshness.entry,
        builtAt: freshness.entryMtimeMs === null ? null : new Date(freshness.entryMtimeMs).toISOString(),
        newestInput: freshness.newestInput,
      };
    }
    /** @type {Record<string, string|undefined>} */
    const env = { ...this.baseEnv, MCP_LOG_LEVEL: 'error' };
    if (this.token !== null) env.MCP_AUTOMATION_CAPABILITY_TOKEN = this.token;

    this.child = this.spawnFn(process.execPath, [BUILD_OUTPUT_ENTRY], {
      cwd: this.cwd, stdio: ['pipe', 'pipe', 'pipe'], env,
    });
    this.child.stdout?.on('data', (chunk) => {
      for (const frame of this.decoder.push(chunk)) this.#route(frame);
    });
    this.child.stderr?.on('data', (chunk) => {
      // Bounded: a server that logs a megabyte must not make the probe the thing
      // that runs out of memory.
      this.stderrTail = `${this.stderrTail}${String(chunk)}`.slice(-8192);
    });

    const initialize = await this.request('initialize', {
      protocolVersion: this.requestedVersion,
      capabilities: {},
      clientInfo: { name: this.clientName, version: '1.0.0' },
    }, options.timeoutMs ?? 60_000);
    if (initialize === null) return { ok: false, reason: 'INITIALIZE_TIMEOUT', pid: this.child?.pid ?? null };
    this.negotiatedVersion = typeof initialize?.result?.protocolVersion === 'string' ? initialize.result.protocolVersion : null;
    this.write({ jsonrpc: '2.0', method: 'notifications/initialized' });
    return {
      ok: initialize.error === undefined,
      reason: initialize.error === undefined ? 'READY' : 'INITIALIZE_ERROR',
      pid: this.child?.pid ?? null,
      requestedVersion: this.requestedVersion,
      negotiatedVersion: this.negotiatedVersion,
      tokenPresented: this.token !== null,
      tokenSource: this.tokenSource,
      error: initialize.error ?? null,
    };
  }

  /** @param {any} frame */
  #route(frame) {
    if (frame && frame.id !== undefined && this.pending.has(frame.id)) {
      const settle = this.pending.get(frame.id);
      this.pending.delete(frame.id);
      settle?.(frame);
      return;
    }
    if (frame && typeof frame.method === 'string') this.notifications.push(frame);
  }

  /** @param {Record<string, unknown>} frame */
  write(frame) {
    this.child?.stdin?.write(encodeFrame(frame));
  }

  /**
   * One correlated JSON-RPC request. Resolves null on timeout — a timeout is a
   * legitimate expected outcome for a corpus scenario, so it must be a value the
   * caller can judge rather than a thrown error that aborts the run.
   * @param {string} method @param {Record<string, unknown>} params @param {number} timeoutMs
   * @returns {Promise<any|null>}
   */
  request(method, params, timeoutMs) {
    const id = ++this.rpcId;
    return new Promise((settle) => {
      const timer = setTimeout(() => { this.pending.delete(id); settle(null); }, timeoutMs);
      this.pending.set(id, (frame) => { clearTimeout(timer); settle(frame); });
      this.write({ jsonrpc: '2.0', id, method, params });
    });
  }

  /**
   * One `tools/call` against the gateway, mirroring NativeDriver.callTool so the
   * corpus runner never branches on transport.
   * @param {Record<string, unknown>} args
   * @param {{ meta?: Record<string, unknown>, task?: Record<string, unknown>,
   *   timeoutMs?: number, toolName?: string }} [options]
   */
  async callTool(args, options = {}) {
    const id = ++this.rpcId;
    /** @type {Record<string, unknown>} */
    const params = { name: options.toolName ?? 'unreal', arguments: args };
    if (options.meta) params._meta = options.meta;
    if (options.task) params.task = options.task;
    const before = this.notifications.length;
    const startedAt = Date.now();
    /** @type {any|null} */
    const frame = await new Promise((settle) => {
      const timer = setTimeout(() => { this.pending.delete(id); settle(null); }, options.timeoutMs ?? 120_000);
      this.pending.set(id, (received) => { clearTimeout(timer); settle(received); });
      this.write({ jsonrpc: '2.0', id, method: 'tools/call', params });
    });
    return {
      requestId: id,
      status: frame === null ? 0 : 200,
      body: frame === null ? 'TIMEOUT' : JSON.stringify(frame),
      response: frame,
      streamNotifications: this.notifications.slice(before),
      frameCount: frame === null ? 0 : 1,
      ms: Date.now() - startedAt,
    };
  }

  /**
   * `notifications/cancelled`. Advisory: reports only that it was SENT. The
   * server forwards it to the bridge (server-factory.ts), but work already
   * dispatched to the game thread cannot be recalled.
   * @param {number} requestId @param {string} reason
   */
  async cancel(requestId, reason) {
    this.write({ jsonrpc: '2.0', method: 'notifications/cancelled', params: { requestId, reason } });
    return { sent: true, status: 200, requestId, reason };
  }

  /**
   * Poll a cheap gateway read until the WebSocket bridge has actually connected.
   * The bridge connects lazily, so without this the first case races the
   * handshake and a disconnected run scores as a wall of failures.
   * @param {{ attempts?: number, intervalMs?: number, timeoutMs?: number }} [options]
   */
  async waitForBridge(options = {}) {
    const attempts = options.attempts ?? 30;
    for (let attempt = 0; attempt < attempts; attempt += 1) {
      const probe = await this.callTool({ operation: 'search', query: 'bridge readiness' }, { timeoutMs: options.timeoutMs ?? 30_000 });
      const text = probe.body ?? '';
      if (probe.response !== null && !/NOT_CONNECTED|UE_NOT_CONNECTED/u.test(text)) {
        return { ready: true, attempts: attempt + 1 };
      }
      if (attempt + 1 < attempts) await new Promise((settle) => { setTimeout(settle, options.intervalMs ?? 2000); });
    }
    return { ready: false, attempts };
  }

  /**
   * Terminate the child and WAIT for it. Resolving before the process is reaped
   * is how a "clean" run still shows an orphan in pgrep: the probe exited first.
   * SIGTERM, then SIGKILL if it does not go.
   * @param {{ graceMs?: number }} [options]
   */
  async close(options = {}) {
    if (this.closed || this.child === null) return { stopped: this.closed, pid: null, signal: null, alreadyClosed: this.closed };
    this.closed = true;
    const child = this.child;
    const pid = child.pid ?? null;
    if (child.exitCode !== null || child.signalCode !== null) {
      return { stopped: true, pid, signal: child.signalCode, alreadyClosed: false };
    }
    /** @type {Promise<{ code: number|null, signal: NodeJS.Signals|null }>} */
    const exited = new Promise((settle) => {
      child.once('exit', (code, signal) => settle({ code, signal }));
    });
    child.kill('SIGTERM');
    const outcome = await Promise.race([
      exited,
      new Promise((settle) => { setTimeout(() => settle(null), options.graceMs ?? 5000); }),
    ]);
    if (outcome === null) {
      child.kill('SIGKILL');
      await exited;
      return { stopped: true, pid, signal: 'SIGKILL', alreadyClosed: false, escalated: true };
    }
    const result = /** @type {{ code: number|null, signal: NodeJS.Signals|null }} */ (outcome);
    return { stopped: true, pid, signal: result.signal, exitCode: result.code, alreadyClosed: false, escalated: false };
  }

  /** Independent check that the spawned child is really gone. */
  verifyChildReleased() {
    if (this.child === null) return { released: true, observed: 'no child was ever spawned' };
    const gone = this.child.exitCode !== null || this.child.signalCode !== null;
    return {
      released: gone,
      observed: gone
        ? `child pid ${this.child.pid} exited (code=${this.child.exitCode}, signal=${this.child.signalCode})`
        : `child pid ${this.child.pid} is STILL RUNNING after close()`,
    };
  }
}
