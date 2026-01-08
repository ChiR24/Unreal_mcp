import fs from 'fs/promises';
import path from 'path';

function usageAndExit(message?: string): never {
  if (message) {
    // eslint-disable-next-line no-console
    console.error(message);
  }
  // eslint-disable-next-line no-console
  console.error(
    [
      'Usage:',
      '  node --loader ts-node/esm scripts/scaffold-tool.ts --tool <toolName> --action <actionName> [--handler <handler-file>]',
      '',
      'Adds a new action to an existing consolidated tool (TS enum + handler switch TODO).',
      'Notes:',
      '  - This script does NOT implement C++ logic; it only scaffolds TS glue and inserts TODO markers.',
      '  - You must add the corresponding C++ handler branch under plugins/McpAutomationBridge.',
    ].join('\n')
  );
  process.exit(2);
}

type Args = {
  tool: string;
  action: string;
  handler?: string;
};

function parseArgs(argv: string[]): Args {
  const out: Partial<Args> = {};
  for (let i = 2; i < argv.length; i += 1) {
    const token = argv[i];
    if (token === '--tool') {
      out.tool = argv[i + 1];
      if (!out.tool) usageAndExit('Missing value after --tool');
      i += 1;
      continue;
    }
    if (token === '--action') {
      out.action = argv[i + 1];
      if (!out.action) usageAndExit('Missing value after --action');
      i += 1;
      continue;
    }
    if (token === '--handler') {
      out.handler = argv[i + 1];
      if (!out.handler) usageAndExit('Missing value after --handler');
      i += 1;
      continue;
    }
    if (token === '-h' || token === '--help') usageAndExit();
    usageAndExit(`Unknown argument: ${token}`);
  }
  if (!out.tool || !out.action) usageAndExit('Both --tool and --action are required');
  return out as Args;
}

function getRepoRootFromImportMeta(importMetaUrl: string): string {
  const script = new URL(importMetaUrl);
  const scriptPath = path.normalize(decodeURIComponent(script.pathname));
  return path.resolve(path.dirname(scriptPath), '..');
}

function escapeRegExp(text: string): string {
  // eslint-disable-next-line no-useless-escape
  return text.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

async function insertActionIntoEnum(defsPath: string, toolName: string, action: string): Promise<void> {
  const src = await fs.readFile(defsPath, 'utf8');

  const toolNeedle = `name: '${toolName}'`;
  const idx = src.indexOf(toolNeedle);
  if (idx === -1) throw new Error(`Could not find tool in consolidated definitions: ${toolNeedle}`);

  // Find the action enum array near this tool block.
  const slice = src.slice(idx, Math.min(src.length, idx + 12000));
  const enumStart = slice.indexOf('enum:');
  if (enumStart === -1) throw new Error(`Could not find action enum for tool '${toolName}'`);

  const bracketStart = slice.indexOf('[', enumStart);
  if (bracketStart === -1) throw new Error(`Could not find '[' for action enum of tool '${toolName}'`);

  // naive bracket match within slice
  let depth = 0;
  let end = -1;
  for (let i = bracketStart; i < slice.length; i += 1) {
    const c = slice[i];
    if (c === '[') depth += 1;
    if (c === ']') {
      depth -= 1;
      if (depth === 0) {
        end = i;
        break;
      }
    }
  }
  if (end === -1) throw new Error(`Could not find closing ']' for enum array of tool '${toolName}'`);

  const enumContent = slice.slice(bracketStart + 1, end);
  const existing = new RegExp(`\\b'${escapeRegExp(action)}'\\b`).test(enumContent);
  if (existing) return;

  // Insert near end, with trailing comma
  const insertion = `${enumContent.trimEnd().endsWith(',') || enumContent.trim().length === 0 ? '' : ','}\n            '${action}'`;
  const updatedEnumContent = enumContent + insertion;
  const newSlice = slice.slice(0, bracketStart + 1) + updatedEnumContent + slice.slice(end);
  const updatedSrc = src.slice(0, idx) + newSlice + src.slice(idx + slice.length);

  await fs.writeFile(defsPath, updatedSrc, 'utf8');
}

async function insertActionCaseIntoHandler(handlerPath: string, action: string): Promise<void> {
  const src = await fs.readFile(handlerPath, 'utf8');
  const needle = 'switch (action)';
  const idx = src.indexOf(needle);
  if (idx === -1) throw new Error(`Could not find 'switch (action)' in handler: ${handlerPath}`);

  if (src.includes(`case '${action}':`)) return;

  const defaultIdx = src.indexOf('default:', idx);
  if (defaultIdx === -1) throw new Error(`Could not find default: in handler: ${handlerPath}`);

  const caseBlock =
    [
      '',
      `    case '${action}':`,
      `      // TODO: implement TS routing for '${action}' (and C++ handler support)`,
      `      return sendRequest('${action}');`,
      '',
    ].join('\n');

  const updated = src.slice(0, defaultIdx) + caseBlock + src.slice(defaultIdx);
  await fs.writeFile(handlerPath, updated, 'utf8');
}

async function main(): Promise<void> {
  const args = parseArgs(process.argv);
  const repoRoot = getRepoRootFromImportMeta(import.meta.url);

  const defsPath = path.join(repoRoot, 'src', 'tools', 'consolidated-tool-definitions.ts');

  // Heuristic: handler file already exists and is named after tool
  const suggestedHandler = path.join(
    repoRoot,
    'src',
    'tools',
    'handlers',
    `${args.tool.replace(/^manage_/, '').replace(/_/g, '-')}-handlers.ts`
  );
  const handlerPath = args.handler
    ? path.isAbsolute(args.handler)
      ? args.handler
      : path.join(repoRoot, args.handler)
    : suggestedHandler;

  await insertActionIntoEnum(defsPath, args.tool, args.action);
  await insertActionCaseIntoHandler(handlerPath, args.action);

  // eslint-disable-next-line no-console
  console.log('Scaffold complete:');
  // eslint-disable-next-line no-console
  console.log(`- Updated: ${path.relative(repoRoot, defsPath)}`);
  // eslint-disable-next-line no-console
  console.log(`- Updated: ${path.relative(repoRoot, handlerPath)}`);
  // eslint-disable-next-line no-console
  console.log('Next: implement corresponding C++ sub-action in plugin handlers and run scripts/verify-handler-sync.ts');
}

main().catch((err: unknown) => {
  const message = err instanceof Error ? err.message : String(err);
  // eslint-disable-next-line no-console
  console.error(`scaffold-tool failed: ${message}`);
  process.exit(1);
});
