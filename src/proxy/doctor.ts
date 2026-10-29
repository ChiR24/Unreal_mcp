/**
 * `unreal-engine-mcp-server doctor`: what the proxy needs, one line each, exiting 1 when something fails: the Node
 * version, the capability token and where it comes from (never its value), an editor answering at UNREAL_MCP_URL
 * (its server and version, the protocol it chose), and whether the proxy and the plugin are the same version.
 */
import { existsSync } from 'node:fs';
import { type IncomingHttpHeaders, request } from 'node:http';
import { join } from 'node:path';

import { PACKAGE } from '../constants.js';
import { projectRootFromEnv } from '../utils/config/ini-reader.js';
import { oneLine } from '../utils/logging/logger.js';
import { readCapabilityToken, sendsTokenTo } from './native-proxy.js';

export interface DoctorLine { level: 'ok' | 'warn' | 'FAIL'; text: string }
interface Reply { status: number; headers: IncomingHttpHeaders; text: string }

const NODE_FLOOR = [20, 19] as const;

/** Whether this Node is at least the package's floor (20.19). */
export function nodeLine(version: string = process.versions.node): DoctorLine {
  const [major = 0, minor = 0] = version.split('.').map(Number);
  const fits = major > NODE_FLOOR[0] || (major === NODE_FLOOR[0] && minor >= NODE_FLOOR[1]);
  return fits ? { level: 'ok', text: `Node ${version}` }
    : { level: 'FAIL', text: `Node ${version} is older than ${NODE_FLOOR.join('.')}: install Node ${NODE_FLOOR.join('.')} or later` };
}

/** Where the token comes from, looked up as readCapabilityToken does; a missing token is a warning (the editor may not ask for one). */
export function tokenLine(token: string | undefined): DoctorLine {
  if (process.env.MCP_AUTOMATION_CAPABILITY_TOKEN) {
    return { level: 'ok', text: 'Capability token from MCP_AUTOMATION_CAPABILITY_TOKEN (its value is not shown)' };
  }
  const root = projectRootFromEnv();
  if (!root) return { level: 'warn', text: 'No capability token: set UE_PROJECT_PATH to the project folder (or MCP_AUTOMATION_CAPABILITY_TOKEN)' };
  if (!existsSync(root)) return { level: 'warn', text: `UE_PROJECT_PATH names ${root}, which does not exist` };
  const file = join(root, 'Saved', 'MCP', 'capability-token');
  return token ? { level: 'ok', text: `Capability token found in ${file} (its value is not shown)` }
    : { level: 'warn', text: `No valid token in ${file}: open the project in the editor once, so the plugin writes it` };
}

function send(url: string, method: string, headers: Record<string, string | number>, body = ''): Promise<Reply> {
  return new Promise((resolve, reject) => {
    const sent = request(url, { method, agent: false, timeout: 5000, headers: { Connection: 'close', ...headers } }, (reply) => {
      let text = '';
      reply.setEncoding('utf8');
      reply.on('data', (chunk: string) => { text += chunk; });
      reply.on('end', () => resolve({ status: reply.statusCode ?? 0, headers: reply.headers, text }));
      reply.on('error', reject);
    });
    sent.on('timeout', () => sent.destroy(new Error('no answer within 5 s')));
    sent.on('error', reject);
    sent.end(body);
  });
}

interface Answer { result?: { serverInfo?: { name?: string; version?: string }; protocolVersion?: string }; error?: { message?: string } }

/** The initialize answer, sent as JSON or as an event stream whose data line holds it. */
function initializeAnswer(text: string): Answer {
  const json = text.trimStart().startsWith('{') ? text : text.split(/\r?\n/u).find((line) => line.startsWith('data:'))?.slice(5) ?? '';
  try { return (JSON.parse(json) as Answer | null) ?? {}; } catch { return {}; }
}

export async function doctorLines(url: string = process.env.UNREAL_MCP_URL ?? 'http://127.0.0.1:3000/mcp'): Promise<DoctorLine[]> {
  const found = await readCapabilityToken();
  const lines = [nodeLine(), tokenLine(found)];
  // The proxy's own rule: the token rides plain HTTP to this machine only.
  const token = sendsTokenTo(url) ? found : undefined;
  if (found && !token) {
    lines.push({ level: 'warn', text: `${url} is not this machine, so the token is not sent over plain HTTP; set UNREAL_MCP_ALLOW_REMOTE_TOKEN=1 to send it anyway` });
  }
  const auth: Record<string, string> = token ? { 'X-MCP-Capability-Token': token } : {};
  const body = JSON.stringify({ jsonrpc: '2.0', id: 1, method: 'initialize', params: { protocolVersion: '2025-11-25',
    capabilities: {}, clientInfo: { name: `${PACKAGE.name} doctor`, version: PACKAGE.version } } });
  let hello: Reply;
  try {
    hello = await send(url, 'POST', { ...auth, 'Content-Type': 'application/json', Accept: 'application/json, text/event-stream',
      'Content-Length': Buffer.byteLength(body) }, body);
  } catch (error) {
    lines.push({ level: 'FAIL', text: `Nothing answers at ${url} (${error instanceof Error ? error.message : String(error)}): start the `
      + 'editor; if it runs, turn on Project Settings > Plugins > EngineWire > Enable Native MCP Server, or set UNREAL_MCP_URL' });
    return lines;
  }
  // The session this opened is closed again, so it does not hold one of the editor's session slots.
  const session = hello.headers['mcp-session-id'];
  if (typeof session === 'string') await send(url, 'DELETE', { ...auth, 'Mcp-Session-Id': session }).catch(() => undefined);
  const { result, error } = initializeAnswer(hello.text);
  const server = result?.serverInfo;
  // What the upstream says is printed on one line each: a line break in its name would forge a doctor line.
  if (hello.status === 401 || hello.status === 403) {
    lines.push({ level: 'FAIL', text: `The editor at ${url} refused this client (HTTP ${hello.status}): the token is missing or not this project's` });
  } else if (!server?.version) {
    lines.push({ level: 'FAIL', text: `${url} answered HTTP ${hello.status}, ${error?.message ? `refusing: ${oneLine(error.message)}` : 'not an MCP initialize reply'}` });
  } else {
    lines.push({ level: 'ok', text: `Editor answers at ${url}: ${oneLine(server.name ?? 'server')} ${oneLine(server.version)}, protocol ${oneLine(result?.protocolVersion ?? '?')}` });
    lines.push(server.version === PACKAGE.version ? { level: 'ok', text: `Proxy and plugin are both ${PACKAGE.version}` }
      : { level: 'warn', text: `Proxy ${PACKAGE.version} and plugin ${oneLine(server.version)} differ: update the older one` });
  }
  return lines;
}

/** Prints the lines and answers the exit code: 1 when one failed. */
export async function runDoctor(write: (text: string) => void = (text) => { process.stdout.write(text); }): Promise<number> {
  const lines = await doctorLines();
  write(`${PACKAGE.name} ${PACKAGE.version} doctor\n${lines.map((line) => `${line.level.padEnd(4)} ${line.text}`).join('\n')}\n`);
  return lines.some((line) => line.level === 'FAIL') ? 1 : 0;
}
