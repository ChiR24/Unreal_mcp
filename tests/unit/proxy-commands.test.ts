// The proxy's one-shot commands: `init` writes a client's MCP config, `doctor` checks what the proxy needs. Real files
// in a temp folder; the doctor's editor check is only run against a port nothing listens on (a live editor is checked
// by hand).
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

import { afterEach, beforeEach, describe, expect, it } from 'vitest';

import { PACKAGE } from '../../src/constants.js';
import { doctorLines, nodeLine } from '../../src/proxy/doctor.js';
import { launch, mergeJson, runInit, tomlBlock } from '../../src/proxy/init.js';

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

  it('fails when nothing answers, and never prints the token', async () => {
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
