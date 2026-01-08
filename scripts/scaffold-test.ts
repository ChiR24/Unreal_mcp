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
      '  node --loader ts-node/esm scripts/scaffold-test.ts --tool <toolName> --action <actionName> [--name "Test name"]',
      '',
      'Appends a new test case entry to tests/integration.mjs.',
    ].join('\n')
  );
  process.exit(2);
}

type Args = { tool: string; action: string; name?: string };

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
    if (token === '--name') {
      out.name = argv[i + 1];
      if (!out.name) usageAndExit('Missing value after --name');
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

async function main(): Promise<void> {
  const args = parseArgs(process.argv);
  const repoRoot = getRepoRootFromImportMeta(import.meta.url);
  const integrationPath = path.join(repoRoot, 'tests', 'integration.mjs');

  const integrationText = await fs.readFile(integrationPath, 'utf8');
  const insertionNeedle = 'const testCases = [';
  const idx = integrationText.indexOf(insertionNeedle);
  if (idx === -1) {
    throw new Error(`Could not find '${insertionNeedle}' in tests/integration.mjs`);
  }

  const defaultName = `${args.tool}:${args.action}`;
  const testName = args.name ?? defaultName;

  const block =
    [
      '',
      '  // ----------------------------------------------------------------------',
      `  // ${testName}`,
      '  // ----------------------------------------------------------------------',
      '  {',
      `    name: ${JSON.stringify(testName)},`,
      `    tool: ${JSON.stringify(args.tool)},`,
      '    args: {',
      `      action: ${JSON.stringify(args.action)},`,
      '      // TODO: add required params for this action',
      '    },',
      '    expectSuccess: true',
      '  },',
    ].join('\n');

  // Insert after the opening bracket line
  const lineBreakIndex = integrationText.indexOf('\n', idx);
  if (lineBreakIndex === -1) {
    throw new Error('Unexpected integration.mjs format; no newline after testCases declaration');
  }

  const newText = integrationText.slice(0, lineBreakIndex + 1) + block + integrationText.slice(lineBreakIndex + 1);
  await fs.writeFile(integrationPath, newText, 'utf8');

  // eslint-disable-next-line no-console
  console.log(`Appended test case to ${path.relative(repoRoot, integrationPath)}`);
}

main().catch((err: unknown) => {
  const message = err instanceof Error ? err.message : String(err);
  // eslint-disable-next-line no-console
  console.error(`scaffold-test failed: ${message}`);
  process.exit(1);
});
