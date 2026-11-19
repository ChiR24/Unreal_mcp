// The proxy's one-shot commands: `init` writes a client's MCP config, `doctor` checks what the proxy needs; and how the
// proxy finds the editor from Saved/MCP/endpoint.json. Real files in a temp folder; the doctor's editor check is only
// run against a port nothing listens on (a live editor is checked by hand).
import { spawnSync } from 'node:child_process';
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

import { afterEach, beforeEach, describe, expect, it } from 'vitest';

import { PACKAGE } from '../../src/constants.js';
import { doctorLines, nodeLine } from '../../src/proxy/doctor.js';
import { chooseEndpoint, DEFAULT_UPSTREAM, editorState, notConnectedText, orderInflight, probePid,
  type EndpointFile } from '../../src/proxy/endpoint.js';
import { launch, mergeJson, runInit, tomlBlock } from '../../src/proxy/init.js';
import { sendsTokenTo } from '../../src/proxy/native-proxy.js';

let folder = '';
beforeEach(() => { folder = mkdtempSync(join(tmpdir(), 'ew-init-')); });
afterEach(() => { rmSync(folder, { recursive: true, force: true }); });

const env = {};
const read = (file: string): Record<string, Record<string, unknown>> => JSON.parse(readFileSync(file, 'utf8'));

describe('init', () => {
  it('shows the entry and writes nothing without --write', () => {
    const result = runInit(['claude', '--project', '/games/demo'], { cwd: folder, env, platform: 'linux' });
    expect(result.ok).toBe(true);
    expect(result.written).toBe(false);
    expect(JSON.parse(result.content).mcpServers['unreal-engine']).toEqual({ command: 'npx', args: ['-y', PACKAGE.name],
      env: { UE_PROJECT_PATH: '/games/demo' } });
    expect(() => readFileSync(join(folder, '.mcp.json'))).toThrow();
  });

  it('keeps the other servers and replaces its own entry', () => {
    const file = join(folder, '.vscode', 'mcp.json');
    mkdirSync(join(folder, '.vscode'));
    writeFileSync(file, '\uFEFF{"inputs": [], "servers": {"other": {"command": "x"}, "unreal-engine": {"command": "old"}}}');
    expect(runInit(['vscode', '--write'], { cwd: folder, env, platform: 'win32' }).written).toBe(true);
    const config = read(file);
    expect(config.inputs).toEqual([]);
    expect(config.servers.other).toEqual({ command: 'x' });
    expect(config.servers['unreal-engine']).toEqual({ type: 'stdio', command: 'cmd', args: ['/c', 'npx', '-y', PACKAGE.name] });
  });

  it('finds the project from a .uproject in the current folder, else says it left it out', () => {
    expect(runInit(['cursor'], { cwd: folder, env, platform: 'linux' }).message).toContain('UE_PROJECT_PATH is left out');
    writeFileSync(join(folder, 'Demo.uproject'), '{}');
    const entry = JSON.parse(runInit(['cursor'], { cwd: folder, env, platform: 'linux' }).content).mcpServers['unreal-engine'];
    expect(entry.env.UE_PROJECT_PATH).toBe(folder.replace(/\\/gu, '/'));
  });

  it('refuses a config it cannot read as JSON, and an unknown client', () => {
    writeFileSync(join(folder, '.mcp.json'), '{ // a comment\n}');
    expect(runInit(['claude', '--write'], { cwd: folder, env }).ok).toBe(false);
    expect(readFileSync(join(folder, '.mcp.json'), 'utf8')).toBe('{ // a comment\n}');
    expect(() => mergeJson('{"mcpServers": []}', { file: '', key: 'mcpServers' }, {})).toThrow();
    expect(runInit(['zed'], { cwd: folder, env }).message).toMatch(/^usage: .* init <claude\|cursor\|vscode\|gemini\|codex>/u);
  });

  it('appends a Codex section in the home config once, and leaves one already there alone', () => {
    const file = join(folder, '.codex', 'config.toml');
    mkdirSync(join(folder, '.codex'));
    writeFileSync(file, 'model = "x"\n');
    expect(runInit(['codex', '--project', '/games/demo', '--write'], { cwd: folder, home: folder, env, platform: 'linux' }).ok).toBe(true);
    expect(readFileSync(file, 'utf8')).toBe(`model = "x"\n\n${tomlBlock('/games/demo', 'linux')}`);
    expect(runInit(['codex', '--write'], { cwd: folder, home: folder, env, platform: 'linux' }).ok).toBe(false);
    writeFileSync(file, '[mcp_servers."unreal-engine".env]\nUE_PROJECT_PATH = "/x"\n');
    expect(runInit(['codex', '--write'], { cwd: folder, home: folder, env, platform: 'linux' }).ok).toBe(false);
    expect(readFileSync(file, 'utf8')).toBe('[mcp_servers."unreal-engine".env]\nUE_PROJECT_PATH = "/x"\n');
    expect(tomlBlock('/games/demo', 'win32')).toBe(`[mcp_servers.unreal-engine]\ncommand = "cmd"\nargs = ["/c", "npx", "-y", "${PACKAGE.name}"]\n`
      + '\n[mcp_servers.unreal-engine.env]\nUE_PROJECT_PATH = "/games/demo"\n');
  });

  it('starts npx through cmd only on Windows', () => {
    expect(launch('win32').command).toBe('cmd');
    expect(launch('darwin')).toEqual({ command: 'npx', args: ['-y', PACKAGE.name] });
  });
});

describe('doctor', () => {
  it('holds Node to the package floor', () => {
    expect(nodeLine('20.19.0').level).toBe('ok');
    expect(nodeLine('22.1.0').level).toBe('ok');
    expect(nodeLine('20.18.3').level).toBe('FAIL');
    expect(nodeLine('18.20.0').level).toBe('FAIL');
  });

  // HTTP carries the token in clear text, so the proxy and doctor send it to this machine only unless the user opts in.
  it('sends the token to a loopback upstream only, unless UNREAL_MCP_ALLOW_REMOTE_TOKEN is 1', () => {
    const before = process.env.UNREAL_MCP_ALLOW_REMOTE_TOKEN;
    delete process.env.UNREAL_MCP_ALLOW_REMOTE_TOKEN;
    try {
      expect(['http://127.0.0.1:3000/mcp', 'http://localhost:3000/mcp', 'http://[::1]:3000/mcp'].map((url) => sendsTokenTo(url)))
        .toEqual([true, true, true]);
      expect(sendsTokenTo('http://192.168.1.20:3000/mcp')).toBe(false);
      expect(sendsTokenTo('http://127.0.0.1.example.com:3000/mcp')).toBe(false);
      process.env.UNREAL_MCP_ALLOW_REMOTE_TOKEN = '1';
      expect(sendsTokenTo('http://192.168.1.20:3000/mcp')).toBe(true);
    } finally {
      if (before === undefined) delete process.env.UNREAL_MCP_ALLOW_REMOTE_TOKEN;
      else process.env.UNREAL_MCP_ALLOW_REMOTE_TOKEN = before;
    }
  });

  it('fails when nothing answers, and never prints the token, at an explicit URL', async () => {
    const before = process.env.MCP_AUTOMATION_CAPABILITY_TOKEN;
    process.env.MCP_AUTOMATION_CAPABILITY_TOKEN = 'a'.repeat(64);
    try {
      const lines = await doctorLines('http://127.0.0.1:1/mcp');
      expect(lines.map((line) => line.level)).toEqual(['ok', 'ok', 'FAIL']);
      expect(JSON.stringify(lines)).not.toContain('a'.repeat(64));
    } finally {
      if (before === undefined) delete process.env.MCP_AUTOMATION_CAPABILITY_TOKEN;
      else process.env.MCP_AUTOMATION_CAPABILITY_TOKEN = before;
    }
  });
});

// The formats the plugin writes under Saved/MCP (the plugin side follows these fixtures).
const mcpFile = (name: string, value: unknown): void => {
  mkdirSync(join(folder, 'Saved', 'MCP'), { recursive: true });
  writeFileSync(join(folder, 'Saved', 'MCP', name), JSON.stringify(value));
};
const crashFolder = (name: string, pid: number, type: string, message: string): void => {
  mkdirSync(join(folder, 'Saved', 'Crashes', name), { recursive: true });
  writeFileSync(join(folder, 'Saved', 'Crashes', name, 'CrashContext.runtime-xml'), '<?xml version="1.0" encoding="UTF-8"?>\n'
    + `<FGenericCrashContext><RuntimeProperties><ProcessId>${pid}</ProcessId><CrashType>${type}</CrashType>`
    + `<ErrorMessage>${message}</ErrorMessage><TimeOfCrash>638990000000000000</TimeOfCrash>`
    + '<EngineVersion>5.8.0-0+UE5</EngineVersion></RuntimeProperties></FGenericCrashContext>');
};
/** The pid of a real process that has exited. */
const deadPid = (): number => {
  const child = spawnSync(process.execPath, ['-e', '']);
  if (!child.pid) throw new Error('no child pid');
  return child.pid;
};
const listening = (pid: number, port = 3123): EndpointFile => ({ pid, port, state: 'listening', pluginVersion: '0.6.1',
  startedAtUtc: '2026-11-19T10:00:00Z' });

describe('endpoint discovery', () => {
  it('keeps the old default without a file, and an explicit URL turns discovery off', () => {
    expect(chooseEndpoint(undefined, undefined)).toEqual({ upstream: DEFAULT_UPSTREAM, port: '3000', inflight: [] });
    expect(chooseEndpoint(undefined, folder).upstream).toBe('http://127.0.0.1:3000/mcp');
    mcpFile('endpoint.json', listening(process.pid));
    expect(chooseEndpoint('http://127.0.0.1:1/mcp', folder)).toEqual({ upstream: 'http://127.0.0.1:1/mcp', port: '1', inflight: [] });
  });

  it('follows a listening editor to its port, and reads a malformed file as no file', () => {
    mcpFile('endpoint.json', listening(process.pid, 3123));
    const found = chooseEndpoint(undefined, folder);
    expect(found).toMatchObject({ upstream: 'http://127.0.0.1:3123/mcp', port: '3123' });
    expect(found.state).toBeUndefined();
    for (const bad of [{ ...listening(process.pid), port: 70000 }, { ...listening(process.pid), state: 'starting' }, { port: 3123 }]) {
      mcpFile('endpoint.json', bad);
      expect(chooseEndpoint(undefined, folder).upstream).toBe(DEFAULT_UPSTREAM);
    }
  });

  it('gives no upstream for a closed editor or a dead pid, and tells crashed from ended', () => {
    mcpFile('endpoint.json', { ...listening(process.pid), state: 'closed_cleanly' });
    const closed = chooseEndpoint(undefined, folder);
    expect(closed).toMatchObject({ port: '3123', state: 'editor_closed' });
    expect(closed.upstream).toBeUndefined();
    const pid = deadPid();
    expect(probePid(pid)).toBe('dead');
    expect(probePid(process.pid)).toBe('alive');
    mcpFile('endpoint.json', listening(pid));
    mcpFile('inflight.json', { entries: [
      { requestId: 'a', capability: 'control_actor.spawn', targetsDigest: 'x', phase: 'dispatch', startedAtUtc: '2026-11-19T10:01:00Z', pid },
      { requestId: 'b', capability: 'control_editor.play', phase: 'dispatch', pid: pid + 1 }] });
    crashFolder('UECC-ensure', pid, 'Ensure', 'an ensure');
    const ended = chooseEndpoint(undefined, folder);
    expect(ended.state).toBe('editor_terminated_unknown');
    expect(ended.upstream).toBeUndefined();
    expect(ended.inflight.map((entry) => entry.capability)).toEqual(['control_actor.spawn']);
    crashFolder('UECC-crash', pid, 'Crash', 'Assertion failed: x &amp; y');
    expect(chooseEndpoint(undefined, folder)).toMatchObject({ state: 'editor_crashed',
      crash: { processId: pid, crashType: 'Crash', errorMessage: 'Assertion failed: x & y' } });
    mcpFile('last-crash.json', { processId: pid, crashType: 'Assert', errorMessage: 'from the next start', timeOfCrash: 't',
      engineVersion: '5.8', inflight: [{ capability: 'manage_level.load', phase: 'dispatch' }] });
    expect(chooseEndpoint(undefined, folder).crash).toMatchObject({ crashType: 'Assert', inflight: [{ capability: 'manage_level.load' }] });
  });

  it('counts EPERM as a live process and decides only from the probe it is given', () => {
    expect(editorState(listening(1), 'eperm')).toBeUndefined();
    expect(editorState(listening(1), 'alive')).toBeUndefined();
    expect(editorState(listening(1), 'dead')).toBe('editor_terminated_unknown');
    expect(editorState(listening(1), 'dead', { processId: 1, crashType: 'Ensure' })).toBe('editor_terminated_unknown');
    expect(editorState({ ...listening(1), state: 'closed_cleanly' }, 'alive')).toBe('editor_closed');
    mcpFile('endpoint.json', listening(4242));
    expect(chooseEndpoint(undefined, folder, () => 'eperm').upstream).toBe('http://127.0.0.1:3123/mcp');
  });

  it('names the dispatch call first, the newest start on a tie, latent calls after', () => {
    expect(orderInflight([
      { capability: 'late', phase: 'latent', startedAtUtc: '2026-11-19T10:05:00Z' },
      { capability: 'old', phase: 'dispatch', startedAtUtc: '2026-11-19T10:00:00Z' },
      { capability: 'new', phase: 'dispatch', startedAtUtc: '2026-11-19T10:02:00Z' }]).map((entry) => entry.capability))
      .toEqual(['new', 'old', 'late']);
  });

  it('doctor sends nothing to a closed or stale endpoint file and says what it reports', async () => {
    const saved = { url: process.env.UNREAL_MCP_URL, project: process.env.UE_PROJECT_PATH };
    delete process.env.UNREAL_MCP_URL;
    process.env.UE_PROJECT_PATH = folder;
    try {
      mcpFile('endpoint.json', listening(deadPid()));
      const stale = await doctorLines();
      expect(stale.at(-1)?.level).toBe('FAIL');
      expect(stale.at(-1)?.text).toMatch(/^Stale endpoint file: Saved\/MCP\/endpoint.json says the editor \(pid \d+\) listened on port 3123/u);
      expect(stale.at(-1)?.text).toContain('Nothing was sent');
      expect(stale.map((line) => line.text).join('\n')).not.toContain('Nothing answers');
      mcpFile('endpoint.json', { ...listening(process.pid), state: 'closed_cleanly' });
      expect((await doctorLines()).at(-1)?.text).toMatch(/^Saved\/MCP\/endpoint.json says the editor closed \(pid \d+, port 3123\)/u);
    } finally {
      if (saved.url === undefined) delete process.env.UNREAL_MCP_URL;
      else process.env.UNREAL_MCP_URL = saved.url;
      if (saved.project === undefined) delete process.env.UE_PROJECT_PATH;
      else process.env.UE_PROJECT_PATH = saved.project;
    }
  });

  it('keeps the old NOT_CONNECTED text without a file, and says what the file reports with one', () => {
    expect(notConnectedText(chooseEndpoint('http://127.0.0.1:1/mcp', undefined))).toBe('NOT_CONNECTED: the Unreal Editor is '
      + 'not running (nothing answers at http://127.0.0.1:1/mcp). Start the editor and call again; this server stays '
      + 'connected, no reconnect is needed. If the editor is running, turn on Project Settings > EngineWire > Enable '
      + 'Native MCP Server and restart it.');
    const file = listening(7);
    const texts = [
      notConnectedText({ port: '3123', file, state: 'editor_closed', inflight: [] }),
      notConnectedText({ port: '3123', file, state: 'editor_terminated_unknown', inflight: [{ capability: 'a.b', phase: 'latent' }] }),
      notConnectedText({ port: '3123', file, state: 'editor_crashed', inflight: [],
        crash: { processId: 7, crashType: 'Crash', errorMessage: 'line one\nline two', inflight: [{ capability: 'c.d', phase: 'dispatch' }] } }),
      notConnectedText({ upstream: 'http://127.0.0.1:3123/mcp', port: '3123', file, inflight: [] })];
    for (const text of texts) {
      expect(text.startsWith('NOT_CONNECTED: Start the editor and call again')).toBe(true);
      expect(text).toContain('delete Saved/MCP/endpoint.json or set UNREAL_MCP_URL');
    }
    expect(texts[0]).toContain('says the editor closed (pid 7, port 3123)');
    expect(texts[1]).toContain('It was waiting on a.b');
    expect(texts[2]).toContain('The editor crashed (pid 7, Crash: line one\\u000aline two). It was running c.d');
    expect(texts[3]).toContain('listens on port 3123, but nothing answers there');
  });
});
