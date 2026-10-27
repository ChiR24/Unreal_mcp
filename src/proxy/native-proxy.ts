/**
 * `unreal-engine-mcp-server`: a stdio MCP server in front of the plugin's native HTTP endpoint that survives
 * editor restarts. The client keeps one session for its whole life: while the editor is down every call answers
 * NOT_CONNECTED, and the first call after it is back opens a new upstream session by itself (a client talking to
 * `/mcp` directly loses its session with the editor and needs a manual reconnect). A 2026-07-28 request carries
 * its own version and needs no session, so it goes upstream on its own with that version in its headers.
 *
 * Env: UNREAL_MCP_URL (default http://127.0.0.1:3000/mcp); UNREAL_MCP_PROXY_CACHE (the editor's last tool
 * list, instructions and capabilities, so a session that starts before the editor still gets the real tool);
 * the capability token (MCP_AUTOMATION_CAPABILITY_TOKEN, else the plugin's token file under UE_PROJECT_PATH),
 * sent as X-MCP-Capability-Token.
 */
import { request } from 'node:http';
import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { readFile } from 'node:fs/promises';
import { homedir } from 'node:os';
import { dirname, join } from 'node:path';
import { createInterface } from 'node:readline';
import type { Readable, Writable } from 'node:stream';

import { PACKAGE } from '../constants.js';
import { projectRootFromEnv } from '../utils/config/ini-reader.js';
import { Logger } from '../utils/logging/logger.js';

type Message = Record<string, unknown> & { id?: unknown; method?: string; params?: Record<string, unknown> };
interface Session { id: string; protocol: string }
interface Upstream { status: number; sessionId?: string; reply?: Message; text: string }
interface Cache { tools?: unknown[]; instructions?: string; capabilities?: Record<string, unknown> }

/** The versions the native endpoint negotiates; a client asking for another gets the newest. */
const NATIVE_PROTOCOLS = ['2025-11-25', '2025-06-18', '2025-03-26'];
const MODERN_VERSION_KEY = 'io.modelcontextprotocol/protocolVersion';

/**
 * The headers native wants on a 2026-07-28 request (McpModern::HeaderMismatch): the body's version and method, and
 * the name it acts on, base64-wrapped when that is not plain ASCII. Undefined for a request of an earlier version.
 */
function modernHeaders(message: Message): Record<string, string> | undefined {
  const version = (message.params?._meta as Record<string, unknown> | undefined)?.[MODERN_VERSION_KEY];
  if (typeof version !== 'string') return undefined;
  const headers: Record<string, string> = { 'MCP-Protocol-Version': version, 'Mcp-Method': message.method ?? '' };
  const field = message.method === 'resources/read' ? 'uri'
    : message.method === 'tools/call' || message.method === 'prompts/get' ? 'name'
      : message.method?.startsWith('tasks/') ? 'taskId' : undefined;
  const name = field ? message.params?.[field] : undefined;
  if (typeof name === 'string' && name) {
    headers['Mcp-Name'] = /^[!-~](?:[ -~]*[!-~])?$/u.test(name) ? name : `=?base64?${Buffer.from(name, 'utf8').toString('base64')}?=`;
  }
  return headers;
}
const PLACEHOLDER_TOOL = {
  name: 'unreal',
  description: 'Unreal Engine editor: operation search, describe, execute or configure. The editor has not been '
    + 'reached yet, so this is a placeholder; the full contract replaces it once the editor answers.',
  inputSchema: { type: 'object', properties: { operation: { type: 'string', enum: ['search', 'describe', 'execute', 'configure'] } }, required: ['operation'] }
};

export interface NativeProxyOptions {
  upstream?: string;
  cacheFile?: string;
  input?: Readable;
  output?: Writable;
  /** Resolves the capability token for each upstream request; defaults to readCapabilityToken. */
  token?: () => Promise<string | undefined>;
}

/**
 * The capability token: MCP_AUTOMATION_CAPABILITY_TOKEN, else the plugin's token file
 * <UE_PROJECT_PATH>/Saved/MCP/capability-token (64 hex). Read on every request, so a token the editor writes after
 * this server starts is picked up. Anything else is undefined, the request goes without one, and the plugin refuses
 * it. The value is never logged.
 */
export async function readCapabilityToken(): Promise<string | undefined> {
  const fromEnv = process.env.MCP_AUTOMATION_CAPABILITY_TOKEN;
  if (fromEnv) return fromEnv;
  const projectRoot = projectRootFromEnv();
  if (!projectRoot) return undefined;
  try {
    const token = (await readFile(join(projectRoot, 'Saved', 'MCP', 'capability-token'), 'utf8')).trim().toLowerCase();
    return /^[0-9a-f]{64}$/u.test(token) ? token : undefined;
  } catch {
    return undefined;
  }
}

export function startNativeProxy(options: NativeProxyOptions = {}): { close: () => void } {
  const upstream = options.upstream ?? process.env.UNREAL_MCP_URL ?? 'http://127.0.0.1:3000/mcp';
  // The cache is served to the client as its tool list and instructions, so it lives in the user's own cache folder:
  // a fixed name in the shared temp dir could be planted by another account on the machine.
  const cacheFile = options.cacheFile ?? process.env.UNREAL_MCP_PROXY_CACHE
    ?? join(process.env.LOCALAPPDATA ?? process.env.XDG_CACHE_HOME ?? join(homedir(), '.cache'), 'unreal-mcp',
      `proxy-${new URL(upstream).port || '80'}.json`);
  const output = options.output ?? process.stdout;
  const log = new Logger('NativeProxy');
  const token = options.token ?? readCapabilityToken;
  const notConnected = `NOT_CONNECTED: the Unreal Editor is not running (nothing answers at ${upstream}). `
    + 'Start the editor and call again; this server stays connected, no reconnect is needed. If the editor is '
    + 'running, turn on Project Settings > EngineWire > Enable Native MCP Server and restart it.';
  const proxyNote = 'This server is reached through the unreal-engine-mcp-server proxy: while the editor is not '
    + 'running every call answers NOT_CONNECTED; once the editor is up the next call reaches it, with no reconnect.';

  let cache: Cache = {};
  try { cache = JSON.parse(readFileSync(cacheFile, 'utf8')) as Cache; } catch { /* first run */ }
  let servedTools: unknown[] = cache.tools ?? [PLACEHOLDER_TOOL];
  let clientProtocol = NATIVE_PROTOCOLS[0];
  let clientReady = false;
  let session: Session | undefined;
  let opening: Promise<Session> | undefined;

  const write = (message: unknown): void => { output.write(`${JSON.stringify(message)}\n`); };
  const remember = (patch: Cache): void => {
    cache = { ...cache, ...patch };
    try {
      mkdirSync(dirname(cacheFile), { recursive: true, mode: 0o700 });
      writeFileSync(cacheFile, JSON.stringify(cache), { mode: 0o600 });
    } catch (error) { log.warn(`cache not written: ${String(error)}`); }
  };

  // One POST per message with Connection: close (pooled sockets trip a libuv assertion on Windows at exit).
  // An SSE reply is parsed as it streams, so progress notifications reach the client while the call runs.
  const post = async (body: Message, current?: Session, onNotification?: (m: Message) => void,
    modern?: Record<string, string>): Promise<Upstream> => {
    const payload = JSON.stringify(body);
    const headers: Record<string, string | number> = {
      'Content-Type': 'application/json',
      Accept: 'application/json, text/event-stream',
      Connection: 'close',
      'Content-Length': Buffer.byteLength(payload),
      ...modern
    };
    const capabilityToken = await token();
    if (capabilityToken) headers['X-MCP-Capability-Token'] = capabilityToken;
    if (current) {
      headers['Mcp-Session-Id'] = current.id;
      headers['MCP-Protocol-Version'] = current.protocol;
    }
    return new Promise((resolve, reject) => {
      const req = request(upstream, { method: 'POST', agent: false, headers, timeout: 600_000 }, (res) => {
        const sse = String(res.headers['content-type'] ?? '').includes('text/event-stream');
        let reply: Message | undefined;
        let text = '';
        let pending = '';
        const take = (message: Message): void => {
          if (message.id !== undefined && message.id === body.id && ('result' in message || 'error' in message)) reply = message;
          else if (message.method && message.id === undefined) onNotification?.(message);
        };
        const takeEvent = (event: string): void => {
          const data = event.split('\n').filter((line) => line.startsWith('data:')).map((line) => line.slice(5).replace(/^ /, '')).join('\n');
          if (data) try { take(JSON.parse(data) as Message); } catch { /* not JSON */ }
        };
        res.setEncoding('utf8');
        res.on('data', (chunk: string) => {
          if (!sse) { text += chunk; return; }
          if (text.length < 2000) text += chunk;
          pending += chunk.replace(/\r/g, '');
          for (let cut = pending.indexOf('\n\n'); cut >= 0; cut = pending.indexOf('\n\n')) {
            takeEvent(pending.slice(0, cut));
            pending = pending.slice(cut + 2);
          }
        });
        res.on('end', () => {
          if (sse && pending.trim()) takeEvent(pending);
          if (!sse && text.trim()) {
            try {
              const parsed = JSON.parse(text) as Message | Message[];
              (Array.isArray(parsed) ? parsed : [parsed]).forEach(take);
            } catch { /* reported by the caller */ }
          }
          const sessionId = res.headers['mcp-session-id'];
          resolve({ status: res.statusCode ?? 0, sessionId: typeof sessionId === 'string' ? sessionId : undefined, reply, text });
        });
        res.on('error', reject);
      });
      req.on('timeout', () => req.destroy(Object.assign(new Error('the editor did not answer within 10 minutes'), { code: 'ETIMEDOUT' })));
      req.on('error', reject);
      req.end(payload);
    });
  };

  const adoptTools = (tools: unknown[], announce: boolean): void => {
    if (JSON.stringify(tools) === JSON.stringify(servedTools)) return;
    servedTools = tools;
    remember({ tools });
    if (announce && clientReady) write({ jsonrpc: '2.0', method: 'notifications/tools/list_changed' });
  };

  const openSession = (): Promise<Session> => {
    if (session) return Promise.resolve(session);
    opening ??= (async () => {
      try {
        const init = await post({ jsonrpc: '2.0', id: 'proxy-initialize', method: 'initialize', params: { protocolVersion: clientProtocol, capabilities: {}, clientInfo: { name: `${PACKAGE.name} proxy`, version: PACKAGE.version } } });
        const result = init.reply?.result as { protocolVersion?: string; instructions?: string; capabilities?: Record<string, unknown>; serverInfo?: { name?: string; version?: string } } | undefined;
        if (!result || !init.sessionId) throw new Error(`initialize answered HTTP ${init.status}: ${init.text.slice(0, 200)}`);
        const opened = { id: init.sessionId, protocol: result.protocolVersion ?? clientProtocol };
        await post({ jsonrpc: '2.0', method: 'notifications/initialized' }, opened);
        session = opened;
        log.info(`connected to ${upstream} (${result.serverInfo?.name ?? '?'} ${result.serverInfo?.version ?? ''})`);
        remember({ instructions: result.instructions, capabilities: result.capabilities });
        // A session that began on the placeholder (or an older cache) learns the real tool now.
        post({ jsonrpc: '2.0', id: 'proxy-tools', method: 'tools/list', params: {} }, opened)
          .then((res) => {
            const tools = (res.reply?.result as { tools?: unknown } | undefined)?.tools;
            if (Array.isArray(tools)) adoptTools(tools, true);
          })
          .catch(() => { /* the next tools/list asks again */ });
        return opened;
      } finally {
        opening = undefined;
      }
    })();
    return opening;
  };

  const forward = async (message: Message): Promise<Upstream> => {
    const modern = modernHeaders(message);
    if (modern) return post(message, undefined, write, modern);
    for (let attempt = 1; ; attempt += 1) {
      const current = await openSession();
      let res: Upstream;
      try {
        res = await post(message, current, write);
      } catch (error) {
        if (session === current) { session = undefined; log.warn(`lost ${upstream}: ${String(error)}`); }
        throw error;
      }
      // 404 = the editor restarted and forgot the session; the call never ran, so open a new one and resend.
      if (res.status === 404 && attempt === 1) {
        if (session === current) session = undefined;
        continue;
      }
      return res;
    }
  };

  const offline = (message: Message, error: unknown): Message => {
    const code = (error as { code?: unknown } | undefined)?.code;
    const why = `${notConnected} (${typeof code === 'string' ? code : String(error)})`;
    const results: Record<string, unknown> = {
      'tools/list': { tools: servedTools },
      'tools/call': { content: [{ type: 'text', text: why }], isError: true },
      'resources/list': { resources: [] },
      'resources/templates/list': { resourceTemplates: [] },
      'prompts/list': { prompts: [] },
      'logging/setLevel': {}
    };
    const result = results[message.method ?? ''];
    if (!result) return { jsonrpc: '2.0', id: message.id, error: { code: -32000, message: why } };
    // A 2026-07-28 result must say it is complete. A server/discover that fails sends the client to initialize.
    return { jsonrpc: '2.0', id: message.id, result: modernHeaders(message) ? { resultType: 'complete', ...result } : result };
  };

  const handle = async (message: Message): Promise<void> => {
    const { id, method } = message;
    if (method === undefined) return; // a response; this proxy never sends the client a request
    if (id === undefined) {
      if (method === 'notifications/initialized') {
        clientReady = true;
        openSession().catch(() => { /* warm-up only */ });
      } else if (modernHeaders(message)) {
        // ponytail: 2026-07-28 has no client notifications over HTTP (closing a call's stream cancels it), so a
        // cancelled modern call runs to the end; destroy its upstream request here if that ever matters.
      } else if (session) {
        post(message, session).catch(() => { /* best effort, e.g. notifications/cancelled */ });
      }
      return;
    }
    if (method === 'initialize') {
      const requested = message.params?.protocolVersion;
      clientProtocol = typeof requested === 'string' && NATIVE_PROTOCOLS.includes(requested) ? requested : NATIVE_PROTOCOLS[0];
      // Resource updates ride the upstream session's GET stream, which this proxy never opens (and a restart replaces
      // that session), so it does not offer resources/subscribe.
      const upstreamCapabilities = cache.capabilities ?? { resources: {}, prompts: {}, completions: {} };
      const { subscribe: _unrelayed, ...resources } = (upstreamCapabilities.resources ?? {}) as Record<string, unknown>;
      write({
        jsonrpc: '2.0',
        id,
        result: {
          protocolVersion: clientProtocol,
          capabilities: { ...upstreamCapabilities, ...('resources' in upstreamCapabilities ? { resources } : {}), tools: { listChanged: true } },
          serverInfo: { name: `${PACKAGE.name} proxy`, version: PACKAGE.version },
          instructions: cache.instructions ? `${cache.instructions}\n\n${proxyNote}` : proxyNote
        }
      });
      return;
    }
    if (method === 'ping') { write({ jsonrpc: '2.0', id, result: {} }); return; }
    try {
      const res = await forward(message);
      const tools = method === 'tools/list' ? (res.reply?.result as { tools?: unknown } | undefined)?.tools : undefined;
      if (Array.isArray(tools)) adoptTools(tools, false); // the client is reading this list already
      write(res.reply ?? { jsonrpc: '2.0', id, error: { code: -32603, message: `the editor answered HTTP ${res.status} with no JSON-RPC reply: ${res.text.slice(0, 300)}` } });
    } catch (error) {
      write(offline(message, error));
    }
  };

  const lines = createInterface({ input: options.input ?? process.stdin, crlfDelay: Infinity });
  lines.on('line', (line) => {
    if (!line.trim()) return;
    let parsed: Message | Message[];
    try { parsed = JSON.parse(line) as Message | Message[]; } catch { write({ jsonrpc: '2.0', id: null, error: { code: -32700, message: 'Parse error' } }); return; }
    for (const message of Array.isArray(parsed) ? parsed : [parsed]) {
      handle(message).catch((error: unknown) => log.error(`unhandled: ${String(error)}`));
    }
  });
  return { close: () => lines.close() };
}
