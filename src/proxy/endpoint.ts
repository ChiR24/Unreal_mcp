/**
 * Where the editor's native /mcp endpoint is and, when it is gone, why. Read from the files the plugin keeps under
 * <UE_PROJECT_PATH>/Saved/MCP: endpoint.json (pid, port, pluginVersion, startedAtUtc, state), inflight.json (the calls
 * it was running) and last-crash.json (the crash its next start found), plus the engine's own Saved/Crashes reports,
 * which exist from the moment of a crash. Read fresh on every use, since the editor rewrites them as it starts, stops
 * and crashes. Without endpoint.json the old rule stands: UNREAL_MCP_URL, else port 3000.
 */
import { readdirSync, readFileSync, statSync } from 'node:fs';
import { join } from 'node:path';

import { oneLine } from '../utils/logging/logger.js';

export const DEFAULT_UPSTREAM = 'http://127.0.0.1:3000/mcp';

export type EditorState = 'editor_listener_stopped' | 'editor_closed' | 'editor_crashed' | 'editor_terminated_unknown';
/** What process.kill(pid, 0) answered: EPERM means the process exists under another account. */
export type PidProbe = 'alive' | 'dead' | 'eperm';

export interface EndpointFile {
  pid: number;
  port: number;
  state: 'listening' | 'closed_cleanly';
  pluginVersion?: string;
  startedAtUtc?: string;
}

export interface InflightEntry {
  capability: string;
  phase: 'dispatch' | 'latent';
  requestId?: string;
  targetsDigest?: string;
  startedAtUtc?: string;
  pid?: number;
}

/** The five crash-report tags, and the calls in flight when the editor went (last-crash.json only). */
export interface CrashReport {
  processId: number;
  crashType?: string;
  errorMessage?: string;
  timeOfCrash?: string;
  engineVersion?: string;
  inflight?: InflightEntry[];
}

export interface EndpointChoice {
  /** Where to send. Undefined: send nothing, the token included; the call answers NOT_CONNECTED. */
  upstream?: string;
  /** Names the cache file, also while there is no upstream. */
  port: string;
  /** Undefined: no usable endpoint.json, so the old default applies. */
  file?: EndpointFile;
  state?: EditorState;
  inflight: InflightEntry[];
  crash?: CrashReport;
}

const MCP_DIR = ['Saved', 'MCP'];
const ENDPOINT_FILE = 'Saved/MCP/endpoint.json';
const text = (value: unknown): string | undefined => (typeof value === 'string' ? value : undefined);
const integer = (value: unknown): number | undefined => (typeof value === 'number' && Number.isInteger(value) ? value : undefined);

function readJson(file: string): Record<string, unknown> | undefined {
  try {
    const parsed: unknown = JSON.parse(readFileSync(file, 'utf8').replace(/^\uFEFF/u, ''));
    return parsed !== null && typeof parsed === 'object' && !Array.isArray(parsed) ? parsed as Record<string, unknown> : undefined;
  } catch {
    return undefined;
  }
}

/** endpoint.json, or undefined when it is missing or not the shape the plugin writes. */
export function readEndpointFile(root: string): EndpointFile | undefined {
  const raw = readJson(join(root, ...MCP_DIR, 'endpoint.json'));
  const pid = integer(raw?.pid);
  const port = integer(raw?.port);
  const state = raw?.state;
  if (!raw || !pid || pid < 1 || !port || port < 1 || port > 65535) return undefined;
  if (state !== 'listening' && state !== 'closed_cleanly') return undefined;
  return { pid, port, state, pluginVersion: text(raw.pluginVersion), startedAtUtc: text(raw.startedAtUtc) };
}

function inflightEntries(value: unknown): InflightEntry[] {
  if (!Array.isArray(value)) return [];
  return value.flatMap((item: unknown): InflightEntry[] => {
    const entry = item !== null && typeof item === 'object' ? item as Record<string, unknown> : {};
    const capability = text(entry.capability);
    const phase = entry.phase;
    if (!capability || (phase !== 'dispatch' && phase !== 'latent')) return [];
    return [{ capability, phase, requestId: text(entry.requestId), targetsDigest: text(entry.targetsDigest),
      startedAtUtc: text(entry.startedAtUtc), pid: integer(entry.pid) }];
  });
}

/** inflight.json's entries for this editor process. */
export function readInflight(root: string, pid: number): InflightEntry[] {
  return inflightEntries(readJson(join(root, ...MCP_DIR, 'inflight.json'))?.entries)
    .filter((entry) => entry.pid === undefined || entry.pid === pid);
}

const XML_ENTITIES: Record<string, string> = { amp: '&', lt: '<', gt: '>', quot: '"', apos: "'" };

function crashXml(file: string): string {
  const bytes = readFileSync(file);
  return bytes[0] === 0xff && bytes[1] === 0xfe ? bytes.toString('utf16le') : bytes.toString('utf8');
}

/**
 * The crash that ended this editor process: last-crash.json once the next start wrote it, else the newest of the
 * reporter's Saved/Crashes folders whose ProcessId is this pid. An ensure writes a report too while the editor lives
 * on, so CrashType Ensure is never a crash.
 */
export function readCrash(root: string, pid: number): CrashReport | undefined {
  const last = readJson(join(root, ...MCP_DIR, 'last-crash.json'));
  if (last && integer(last.processId) === pid && last.crashType !== 'Ensure') {
    return { processId: pid, crashType: text(last.crashType), errorMessage: text(last.errorMessage),
      timeOfCrash: text(last.timeOfCrash), engineVersion: text(last.engineVersion), inflight: inflightEntries(last.inflight) };
  }
  const crashes = join(root, 'Saved', 'Crashes');
  let reports: { file: string; time: number }[];
  try {
    reports = readdirSync(crashes, { withFileTypes: true }).filter((entry) => entry.isDirectory()).flatMap((entry) => {
      const file = join(crashes, entry.name, 'CrashContext.runtime-xml');
      try { return [{ file, time: statSync(file).mtimeMs }]; } catch { return []; }
    });
  } catch {
    return undefined;
  }
  // ponytail: the 32 newest reports only; a project that keeps hundreds of old crash folders never needs more.
  for (const { file } of reports.sort((a, b) => b.time - a.time).slice(0, 32)) {
    let xml: string;
    try { xml = crashXml(file); } catch { continue; }
    const tag = (name: string): string | undefined => {
      const found = new RegExp(`<${name}>([^<]*)</${name}>`, 'u').exec(xml)?.[1];
      return found?.replace(/&(amp|lt|gt|quot|apos);/gu, (_, entity: string) => XML_ENTITIES[entity] ?? '');
    };
    if (Number(tag('ProcessId')) !== pid || tag('CrashType') === 'Ensure') continue;
    return { processId: pid, crashType: tag('CrashType'), errorMessage: tag('ErrorMessage'), timeOfCrash: tag('TimeOfCrash'),
      engineVersion: tag('EngineVersion') };
  }
  return undefined;
}

/** Whether a process with this pid exists. Assumes the proxy and the editor share a pid namespace (not WSL, not a container). */
export function probePid(pid: number): PidProbe {
  try {
    process.kill(pid, 0);
    return 'alive';
  } catch (error) {
    return (error as NodeJS.ErrnoException).code === 'EPERM' ? 'eperm' : 'dead';
  }
}

/**
 * The editor's state when endpoint.json rules out a request, from the file, what probing its pid answered and its
 * crash report. Undefined while it may be listening: HTTP decides then.
 */
export function editorState(file: EndpointFile, pid: PidProbe, crash?: CrashReport): EditorState | undefined {
  if (file.state === 'closed_cleanly') return 'editor_closed';
  if (pid !== 'dead') return undefined;
  return crash?.processId === file.pid && crash.crashType !== 'Ensure' ? 'editor_crashed' : 'editor_terminated_unknown';
}

/**
 * Where to send. An explicit upstream (an option or UNREAL_MCP_URL) wins and turns discovery off. Else, with a project
 * folder, endpoint.json picks the port, and a file whose editor closed or whose pid is dead gives no upstream at all: its
 * port may have a new owner, which must not get the token. No file: the old default.
 */
export function chooseEndpoint(explicit: string | undefined, root: string | undefined,
  probe: (pid: number) => PidProbe = probePid): EndpointChoice {
  const plain = (upstream: string): EndpointChoice => ({ upstream, port: new URL(upstream).port || '80', inflight: [] });
  if (explicit) return plain(explicit);
  const file = root ? readEndpointFile(root) : undefined;
  if (!root || !file) return plain(DEFAULT_UPSTREAM);
  const port = String(file.port);
  const pid = file.state === 'listening' ? probe(file.pid) : 'dead';
  const crash = file.state === 'listening' && pid === 'dead' ? readCrash(root, file.pid) : undefined;
  const state = editorState(file, pid, crash);
  if (!state) return { upstream: `http://127.0.0.1:${port}/mcp`, port, file, inflight: [] };
  return { port, file, state, inflight: state === 'editor_closed' ? [] : readInflight(root, file.pid), crash };
}

/** Dispatch entries first, the newest start first, then latent ones: the call the editor was running when it went. */
export function orderInflight(entries: readonly InflightEntry[]): InflightEntry[] {
  const rank = (entry: InflightEntry): number => (entry.phase === 'dispatch' ? 0 : 1);
  const start = (entry: InflightEntry): string => entry.startedAtUtc ?? '';
  return [...entries].sort((a, b) => rank(a) - rank(b) || (start(a) < start(b) ? 1 : start(a) > start(b) ? -1 : 0));
}

const clip = (value: string, max: number): string => {
  const line = oneLine(value);
  return line.length > max ? `${line.slice(0, max)}...` : line;
};

function crashDetail(crash: CrashReport | undefined): string {
  const said = [crash?.crashType, crash?.errorMessage].filter((part): part is string => Boolean(part)).map((part) => clip(part, 300));
  return `${said.length ? `, ${said.join(': ')}` : ''}${crash?.timeOfCrash ? `, at ${clip(crash.timeOfCrash, 40)}` : ''}`;
}

function inflightText(entries: readonly InflightEntry[]): string {
  const [first, ...rest] = orderInflight(entries);
  if (!first) return '';
  const name = (entry: InflightEntry): string => clip(entry.capability, 120);
  const started = first.startedAtUtc ? ` (started ${clip(first.startedAtUtc, 40)})` : '';
  const others = rest.slice(0, 5).map(name).join(', ') + (rest.length > 5 ? ` and ${rest.length - 5} more` : '');
  return ` It was ${first.phase === 'dispatch' ? 'running' : 'waiting on'} ${name(first)}${started}`
    + `${rest.length ? `; also waiting on ${others}` : ''}: check what that call changed before running it again.`;
}

/**
 * What endpoint.json and the crash files say about an editor that does not answer: its state (a choice with an upstream
 * that was refused is a stopped listener), and for a crashed or ended one the calls it was running.
 */
export function fileReport(file: EndpointFile, choice: EndpointChoice): string {
  const reports: Record<EditorState, string> = {
    editor_closed: `${ENDPOINT_FILE} says the editor closed (pid ${file.pid}, port ${file.port}).`,
    editor_crashed: `The editor crashed (pid ${file.pid}${crashDetail(choice.crash)}).`,
    editor_terminated_unknown: `${ENDPOINT_FILE} says the editor (pid ${file.pid}) listened on port ${file.port}, but that `
      + 'process has ended with no crash report yet.',
    editor_listener_stopped: `${ENDPOINT_FILE} says the editor (pid ${file.pid}) listens on port ${file.port}, but nothing `
      + 'answers there: turn on Project Settings > EngineWire > Enable Native MCP Server and restart it.'
  };
  const state = choice.state ?? 'editor_listener_stopped';
  const running = state === 'editor_crashed' || state === 'editor_terminated_unknown'
    ? inflightText(choice.crash?.inflight?.length ? choice.crash.inflight : choice.inflight) : '';
  return `${reports[state]}${running}`;
}

/** How to get past an endpoint.json that a plugin which no longer writes it left behind. */
export const STALE_FILE_WAY_OUT = `delete ${ENDPOINT_FILE} or set UNREAL_MCP_URL`;

const ADVICE = 'Start the editor and call again; this server stays connected, no reconnect is needed.';

/**
 * The NOT_CONNECTED text. Every variant starts "NOT_CONNECTED: " (the server instructions and the use count key on it).
 * Without endpoint.json it is the text this proxy always sent; with one it says what the file reports, and how to get
 * past a file that a plugin which no longer writes it left behind.
 */
export function notConnectedText(choice: EndpointChoice): string {
  const { file } = choice;
  if (!file) {
    return `NOT_CONNECTED: the Unreal Editor is not running (nothing answers at ${choice.upstream ?? DEFAULT_UPSTREAM}). ${ADVICE} `
      + 'If the editor is running, turn on Project Settings > EngineWire > Enable Native MCP Server and restart it.';
  }
  return `NOT_CONNECTED: ${ADVICE} ${fileReport(file, choice)} If this goes on while the editor runs, its plugin may no `
    + `longer write that file: ${STALE_FILE_WAY_OUT}.`;
}
