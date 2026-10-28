/**
 * `unreal-engine-mcp-server init <client> [--project <folder>] [--write]`: a client's MCP config with this server in it
 * as "unreal-engine", always shown first and written only with --write. Other servers in the file stay as they are and
 * an "unreal-engine" entry already there is replaced. Project files go in the current folder; Codex keeps one config in
 * the user's home, and a section already there is left for the user to edit (its TOML is not rewritten).
 */
import { existsSync, mkdirSync, readFileSync, readdirSync, writeFileSync } from 'node:fs';
import { homedir } from 'node:os';
import { dirname, isAbsolute, join, resolve } from 'node:path';

import { PACKAGE } from '../constants.js';

interface ClientConfig { file: string; key?: string; type?: string; home?: boolean; toml?: boolean }

export const CLIENTS: Readonly<Record<string, ClientConfig>> = {
  claude: { file: '.mcp.json', key: 'mcpServers' },
  cursor: { file: '.cursor/mcp.json', key: 'mcpServers' },
  vscode: { file: '.vscode/mcp.json', key: 'servers', type: 'stdio' },
  gemini: { file: '.gemini/settings.json', key: 'mcpServers' },
  codex: { file: '.codex/config.toml', home: true, toml: true },
};
const NAME = 'unreal-engine';

export interface InitOptions { cwd?: string; home?: string; env?: NodeJS.ProcessEnv; platform?: string }
export interface InitResult { ok: boolean; file: string; content: string; written: boolean; message: string }

/** The project folder: --project, else UE_PROJECT_PATH, else the current folder when it holds a .uproject. */
export function projectFor(argument: string | undefined, cwd: string, env: NodeJS.ProcessEnv): string | undefined {
  const named = argument ?? env.UE_PROJECT_PATH;
  if (named) return (isAbsolute(named) ? named : resolve(cwd, named)).replace(/\\/gu, '/');
  return readdirSync(cwd).some((file) => file.toLowerCase().endsWith('.uproject')) ? cwd.replace(/\\/gu, '/') : undefined;
}

/** npx is a batch file on Windows, which a client starting servers without a shell cannot run: cmd /c runs it. */
export function launch(platform: string): { command: string; args: string[] } {
  const npx = ['-y', PACKAGE.name];
  return platform === 'win32' ? { command: 'cmd', args: ['/c', 'npx', ...npx] } : { command: 'npx', args: npx };
}

export function serverEntry(client: ClientConfig, project: string | undefined, platform: string): Record<string, unknown> {
  return { ...(client.type ? { type: client.type } : {}), ...launch(platform), ...(project ? { env: { UE_PROJECT_PATH: project } } : {}) };
}

const isObject = (value: unknown): value is Record<string, unknown> => typeof value === 'object' && value !== null && !Array.isArray(value);

/** The JSON config with the entry merged in; throws when the file there is not a plain JSON object. */
export function mergeJson(existing: string | undefined, client: ClientConfig, entry: Record<string, unknown>): string {
  const text = existing?.replace(/^\uFEFF/u, '').trim();
  const config: unknown = text ? JSON.parse(text) : {};
  const key = client.key ?? 'mcpServers';
  const servers: unknown = isObject(config) ? config[key] ?? {} : undefined;
  if (!isObject(config) || !isObject(servers)) throw new Error(`not an object with "${key}"`);
  config[key] = { ...servers, [NAME]: entry };
  return `${JSON.stringify(config, null, 2)}\n`;
}

/** TOML basic strings take JSON's escapes, so JSON.stringify quotes each value. */
export function tomlBlock(project: string | undefined, platform: string): string {
  const { command, args } = launch(platform);
  return `[mcp_servers.${NAME}]\ncommand = ${JSON.stringify(command)}\nargs = [${args.map((arg) => JSON.stringify(arg)).join(', ')}]\n`
    + (project ? `\n[mcp_servers.${NAME}.env]\nUE_PROJECT_PATH = ${JSON.stringify(project)}\n` : '');
}

export function runInit(argv: readonly string[], options: InitOptions = {}): InitResult {
  const cwd = options.cwd ?? process.cwd();
  const env = options.env ?? process.env;
  const platform = options.platform ?? process.platform;
  let name: string | undefined;
  let projectArg: string | undefined;
  for (let index = 0; index < argv.length; index++) {
    if (argv[index] === '--project') projectArg = argv[++index];
    else if (!argv[index].startsWith('--')) name ??= argv[index];
  }
  const client = name ? CLIENTS[name] : undefined;
  if (!client) {
    return { ok: false, file: '', content: '', written: false, message: `usage: ${PACKAGE.name} init <${Object.keys(CLIENTS).join('|')}> `
      + '[--project <folder>] [--write]' };
  }
  const project = projectFor(projectArg, cwd, env);
  const file = join(client.home ? options.home ?? homedir() : cwd, client.file);
  const existing = existsSync(file) ? readFileSync(file, 'utf8') : undefined;
  let content: string;
  if (client.toml) {
    // Its table in any spelling TOML allows (a quoted key, or only its env table): a second one would not load.
    if (existing && /^\s*\[\s*mcp_servers\s*\.\s*"?unreal-engine"?\s*[\].]/mu.test(existing)) {
      return { ok: false, file, content: tomlBlock(project, platform), written: false,
        message: `${file} already has [mcp_servers.${NAME}]; it is left as it is, so edit it to read as above` };
    }
    content = `${existing?.trim() ? `${existing.trimEnd()}\n\n` : ''}${tomlBlock(project, platform)}`;
  } else {
    try { content = mergeJson(existing, client, serverEntry(client, project, platform)); } catch {
      return { ok: false, file, content: '', written: false,
        message: `${file} is not a plain JSON object with "${client.key}" (comments?): add the "${NAME}" entry by hand` };
    }
  }
  const write = argv.includes('--write');
  if (write) {
    mkdirSync(dirname(file), { recursive: true });
    writeFileSync(file, content);
  }
  const noProject = project ? '' : ' No project folder was found, so UE_PROJECT_PATH is left out: pass --project <folder>.';
  return { ok: true, file, content, written: write,
    message: (write ? `Written to ${file}.` : `Not written: run again with --write to save it to ${file}.`) + noProject };
}
