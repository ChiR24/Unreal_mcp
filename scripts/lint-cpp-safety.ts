/* eslint-disable no-console */
import fs from 'fs/promises';
import path from 'path';

type Finding = {
  file: string;
  line: number;
  column: number;
  pattern: string;
  recommendation: string;
  lineText: string;
};

type Output = {
  root: string;
  findings: Finding[];
};

function usageAndExit(): never {
  console.error(
    [
      'Usage:',
      '  node --loader ts-node/esm scripts/lint-cpp-safety.ts [--format text|json]',
      '',
      'Scans the UE plugin C++ source for unsafe patterns.',
    ].join('\n')
  );
  process.exit(2);
}

function parseArgs(argv: string[]): { format: 'text' | 'json' } {
  const out: { format: 'text' | 'json' } = { format: 'text' };
  for (let i = 2; i < argv.length; i += 1) {
    const token = argv[i];
    if (token === '--format') {
      const value = argv[i + 1];
      if (value !== 'text' && value !== 'json') usageAndExit();
      out.format = value;
      i += 1;
      continue;
    }
    if (token === '-h' || token === '--help') usageAndExit();
    usageAndExit();
  }
  return out;
}

async function walk(dir: string): Promise<string[]> {
  const out: string[] = [];
  const entries = await fs.readdir(dir, { withFileTypes: true });
  for (const e of entries) {
    const full = path.join(dir, e.name);
    if (e.isDirectory()) {
      out.push(...(await walk(full)));
    } else {
      out.push(full);
    }
  }
  return out;
}

function toPosixPath(p: string): string {
  return p.replace(/\\/g, '/');
}

function scanText(fileRel: string, text: string): Finding[] {
  const rules: Array<{ pattern: RegExp; label: string; recommendation: string }> = [
    {
      pattern: /\bUPackage::SavePackage\b/g,
      label: 'UPackage::SavePackage',
      recommendation: 'UE 5.7+ safety: use McpSafeAssetSave(...) helper instead of UPackage::SavePackage().',
    },
    {
      pattern: /\bGWorld\b/g,
      label: 'GWorld',
      recommendation: 'Avoid GWorld; use GetActiveWorld() from McpAutomationBridgeHelpers.h.',
    },
    {
      pattern: /\bFindActorByName\b/g,
      label: 'FindActorByName',
      recommendation: 'Prefer FindActorByLabelOrName<T>(...) from McpAutomationBridgeHelpers.h.',
    },
    {
      pattern: /\bTObjectIterator\b/g,
      label: 'TObjectIterator',
      recommendation: 'Avoid TObjectIterator for performance; use UWorld-scoped iterators or Asset Registry.',
    },
  ];

  const lines = text.split(/\r?\n/);
  const findings: Finding[] = [];

  for (let lineIndex = 0; lineIndex < lines.length; lineIndex += 1) {
    const lineText = lines[lineIndex];
    for (const rule of rules) {
      rule.pattern.lastIndex = 0;
      for (let m = rule.pattern.exec(lineText); m; m = rule.pattern.exec(lineText)) {
        findings.push({
          file: fileRel,
          line: lineIndex + 1,
          column: m.index + 1,
          pattern: rule.label,
          recommendation: rule.recommendation,
          lineText,
        });
      }
    }
  }

  return findings;
}

import { fileURLToPath } from 'url';

// ...

async function main(): Promise<void> {
  const { format } = parseArgs(process.argv);

  const __filename = fileURLToPath(import.meta.url);
  const __dirname = path.dirname(__filename);
  const repoRoot = path.resolve(__dirname, '..');
  const pluginRoot = path.join(
    repoRoot,
    'plugins',
    'McpAutomationBridge',
    'Source',
    'McpAutomationBridge'
  );

  const all = await walk(pluginRoot);
  const candidates = all.filter((p) => p.endsWith('.cpp') || p.endsWith('.h'));

  const findings: Finding[] = [];
  for (const abs of candidates) {
    const rel = toPosixPath(path.relative(repoRoot, abs));
    const text = await fs.readFile(abs, 'utf8');
    findings.push(...scanText(rel, text));
  }

  const output: Output = { root: toPosixPath(path.relative(process.cwd(), pluginRoot)), findings };

  if (format === 'json') {
    console.log(JSON.stringify(output, null, 2));
  } else {
    if (findings.length === 0) {
      console.log('OK: No forbidden UE safety patterns found.');
    } else {
      console.log(`Found ${findings.length} issue(s):`);
      for (const f of findings) {
        console.log(`- ${f.file}:${f.line}:${f.column} [${f.pattern}] ${f.lineText.trim()}`);
        console.log(`  -> ${f.recommendation}`);
      }
    }
  }

  process.exit(findings.length > 0 ? 1 : 0);
}

main().catch((err: unknown) => {
  const message = err instanceof Error ? err.message : String(err);
  console.error(`lint-cpp-safety failed: ${message}`);
  process.exit(1);
});
