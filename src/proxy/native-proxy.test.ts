import { createServer, type IncomingHttpHeaders, type Server } from 'node:http';
import type { AddressInfo } from 'node:net';
import { rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { PassThrough } from 'node:stream';
import { createInterface } from 'node:readline';
import { afterEach, describe, expect, it } from 'vitest';

import { startNativeProxy } from './native-proxy.js';

type Msg = Record<string, unknown>;

/** The value at a path through nested objects and arrays, or undefined. */
const dig = (value: unknown, ...path: Array<string | number>): unknown =>
  path.reduce<unknown>((cur, key) => (cur !== null && typeof cur === 'object' ? (cur as Record<string | number, unknown>)[key] : undefined), value);

/** A fake editor endpoint with the native session rules: 404 for a session it does not know. */
function fakeEditor() {
  let sessions = new Set<string>();
  let server: Server | undefined;
  let port = 0;
  const seenHeaders: IncomingHttpHeaders[] = [];
  const start = () => new Promise<void>((resolve) => {
    const listening = createServer((req, res) => {
      let body = '';
      req.on('data', (chunk) => { body += chunk; });
      req.on('end', () => {
        seenHeaders.push(req.headers);
        const msg = JSON.parse(body) as Msg;
        const json = (status: number, payload: unknown, headers: Record<string, string> = {}) => {
          res.writeHead(status, { 'Content-Type': 'application/json', ...headers });
          res.end(JSON.stringify(payload));
        };
        if (msg.method === 'initialize') {
          const id = `S${sessions.size}-${Date.now()}`;
          sessions.add(id);
          json(200, { jsonrpc: '2.0', id: msg.id, result: { protocolVersion: dig(msg, 'params', 'protocolVersion'), capabilities: { tools: {} }, serverInfo: { name: 'fake', version: '9' }, instructions: 'FAKE INSTRUCTIONS' } }, { 'Mcp-Session-Id': id });
          return;
        }
        if (!sessions.has(String(req.headers['mcp-session-id']))) { json(404, { jsonrpc: '2.0', id: msg.id ?? null, error: { code: -32600, message: 'Invalid or expired session ID' } }); return; }
        if (msg.id === undefined) { res.writeHead(202); res.end(); return; }
        if (msg.method === 'tools/list') { json(200, { jsonrpc: '2.0', id: msg.id, result: { tools: [{ name: 'unreal', description: 'FAKE TOOL', inputSchema: { type: 'object' } }] } }); return; }
        if (msg.method === 'tools/call') {
          res.writeHead(200, { 'Content-Type': 'text/event-stream' });
          res.write(`data: ${JSON.stringify({ jsonrpc: '2.0', method: 'notifications/progress', params: { progressToken: dig(msg, 'params', '_meta', 'progressToken'), progress: 1 } })}\n\n`);
          res.end(`data: ${JSON.stringify({ jsonrpc: '2.0', id: msg.id, result: { content: [{ type: 'text', text: `echo:${JSON.stringify(dig(msg, 'params', 'arguments'))}` }] } })}\n\n`);
          return;
        }
        json(200, { jsonrpc: '2.0', id: msg.id, error: { code: -32601, message: 'Method not found' } });
      });
    });
    server = listening;
    listening.listen(port, '127.0.0.1', () => { port = (listening.address() as AddressInfo).port; resolve(); });
  });
  const stop = () => new Promise<void>((resolve) => { server?.closeAllConnections(); server?.close(() => resolve()); });
  return { start, stop, forgetSessions: () => { sessions = new Set(); }, url: () => `http://127.0.0.1:${port}/mcp`, seenHeaders };
}

function proxyClient(upstream: string, cacheFile: string) {
  const input = new PassThrough();
  const output = new PassThrough();
  const seen: Msg[] = [];
  const waiters: Array<{ match: (m: Msg) => boolean; resolve: (m: Msg) => void }> = [];
  createInterface({ input: output }).on('line', (line) => {
    const msg = JSON.parse(line) as Msg;
    seen.push(msg);
    for (const w of [...waiters]) if (w.match(msg)) { waiters.splice(waiters.indexOf(w), 1); w.resolve(msg); }
  });
  const proxy = startNativeProxy({ upstream, cacheFile, input, output, token: async () => 'secret-token' });
  let nextId = 1;
  const waitFor = (match: (m: Msg) => boolean) => new Promise<Msg>((resolve, reject) => {
    const hit = seen.find(match);
    if (hit) { resolve(hit); return; }
    const timer = setTimeout(() => reject(new Error('timed out waiting for a proxy message')), 4000);
    waiters.push({ match, resolve: (m) => { clearTimeout(timer); resolve(m); } });
  });
  const send = (msg: Msg) => input.write(`${JSON.stringify(msg)}\n`);
  const call = (method: string, params: Msg = {}) => { const id = nextId++; send({ jsonrpc: '2.0', id, method, params }); return waitFor((m) => m.id === id); };
  return { waitFor, send, call, close: () => proxy.close() };
}

const unreal = (args: Msg, token?: string) => ({ name: 'unreal', arguments: args, ...(token ? { _meta: { progressToken: token } } : {}) });
const text = (reply: Msg) => String(dig(reply, 'result', 'content', 0, 'text'));
const firstTool = (reply: Msg) => String(dig(reply, 'result', 'tools', 0, 'description'));

describe('native proxy: one client session that outlives the editor', () => {
  const cacheFile = join(tmpdir(), `native-proxy-test-${process.pid}.json`);
  afterEach(() => rmSync(cacheFile, { force: true }));

  it('answers while the editor is down, reaches it once it is up, and survives a restart', async () => {
    const editor = fakeEditor();
    await editor.start();
    const url = editor.url();
    await editor.stop();
    const p = proxyClient(url, cacheFile);

    const init = await p.call('initialize', { protocolVersion: '2024-11-05', capabilities: {}, clientInfo: { name: 'test', version: '1' } });
    expect(dig(init, 'result', 'protocolVersion'), 'a legacy version is answered with the newest native one').toBe('2025-11-25');
    expect(dig(init, 'result', 'capabilities', 'tools', 'listChanged')).toBe(true);
    p.send({ jsonrpc: '2.0', method: 'notifications/initialized' });
    expect(firstTool(await p.call('tools/list'))).toMatch(/placeholder/);
    const down = await p.call('tools/call', unreal({ operation: 'search' }));
    expect(dig(down, 'result', 'isError')).toBe(true);
    expect(text(down)).toMatch(/^NOT_CONNECTED/);
    expect(dig(await p.call('completion/complete'), 'error', 'code')).toBe(-32000);

    await editor.start();
    const up = await p.call('tools/call', unreal({ x: 1 }, 'tok1'));
    expect(text(up)).toBe('echo:{"x":1}');
    expect(dig(await p.waitFor((m) => m.method === 'notifications/progress'), 'params', 'progressToken')).toBe('tok1');
    await p.waitFor((m) => m.method === 'notifications/tools/list_changed');
    expect(firstTool(await p.call('tools/list'))).toBe('FAKE TOOL');
    expect(editor.seenHeaders.at(-1)?.['x-mcp-capability-token'], 'the token rides on every upstream request').toBe('secret-token');

    editor.forgetSessions();
    expect(text(await p.call('tools/call', unreal({ x: 2 }))), 'a restarted editor gets a new session and the call').toBe('echo:{"x":2}');

    await editor.stop();
    expect(text(await p.call('tools/call', unreal({ x: 3 })))).toMatch(/^NOT_CONNECTED/);
    expect(firstTool(await p.call('tools/list')), 'the real tool stays listed').toBe('FAKE TOOL');
    await editor.start();
    expect(text(await p.call('tools/call', unreal({ x: 4 })))).toBe('echo:{"x":4}');
    p.close();
    await editor.stop();

    const q = proxyClient(url, cacheFile);
    const again = await q.call('initialize', { protocolVersion: '2025-06-18', capabilities: {}, clientInfo: { name: 'test', version: '1' } });
    expect(dig(again, 'result', 'protocolVersion')).toBe('2025-06-18');
    expect(String(dig(again, 'result', 'instructions')), 'a session started while the editor is down gets the cached instructions').toMatch(/FAKE INSTRUCTIONS[\s\S]*proxy/);
    expect(firstTool(await q.call('tools/list'))).toBe('FAKE TOOL');
    q.close();
  });
});
