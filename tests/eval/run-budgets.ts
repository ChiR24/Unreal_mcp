// tests/eval/run-budgets.ts
// Manual/CI entrypoint for the Task-48 release gate.
//
// Exit code is the machine result: 0 only when every declared budget holds.
// A breach writes the full report and exits 1, so the artifact that proves the
// failure is always produced.
//
// This driver deliberately does NOT spawn `dist/cli.js`. It measures the
// gateway in-process from the working tree, which is the tree the report's
// `treeHash` covers. Spawning the build would measure whatever `dist/` last
// compiled — the exact stale-artifact confusion `assertDistFresh` exists to
// refuse — so the honest fix here is not to depend on the build at all.

import { writeFileSync } from 'node:fs';
import { buildTask48Report } from './report.js';
import { BudgetError } from './budgets.js';
import { CorpusValidationError } from './errors.js';

type Args = { readonly output: string; readonly manifest: string | undefined };

function parseArgs(argv: readonly string[]): Args {
  let output = '.omo/evidence/task-48-budget-report.json';
  let manifest: string | undefined;
  for (let index = 0; index < argv.length; index += 1) {
    const flag = argv[index];
    const value = argv[index + 1];
    if (flag === '--output') {
      if (value === undefined) throw new BudgetError('--output requires a path');
      output = value;
      index += 1;
    } else if (flag === '--manifest') {
      if (value === undefined) throw new BudgetError('--manifest requires a path');
      manifest = value;
      index += 1;
    }
  }
  return { output, manifest };
}

function main(): void {
  const args = parseArgs(process.argv.slice(2));
  const report = buildTask48Report(args.manifest === undefined ? {} : { manifestPath: args.manifest });
  writeFileSync(args.output, `${JSON.stringify(report, null, 2)}\n`, 'utf8');

  const lines = [
    `Task48 budgets: verdict=${report.verdict} deterministicHash=${report.deterministicHash.slice(0, 16)}`,
    `  tree=${report.treeHash.slice(0, 16)} (${report.treeHashInputCount} inputs) corpus=${report.corpusScorer.corpusHash.slice(0, 16)} records=${report.registryRecordCount}`,
    `  model=${report.model.status}`,
  ];
  for (const budget of report.budgets) {
    lines.push(
      `  [${budget.passed ? 'PASS' : 'FAIL'}] ${budget.id} observed=${budget.observed} `
      + `${budget.direction} ${budget.threshold} (${budget.unit}, ${budget.kind})`,
    );
  }
  process.stdout.write(`${lines.join('\n')}\n`);
  if (report.verdict !== 'PASS') process.exitCode = 1;
}

try {
  main();
} catch (error) {
  const message = error instanceof CorpusValidationError || error instanceof BudgetError
    ? error.message
    : error instanceof Error ? error.message : 'unknown error';
  process.stderr.write(`Task48 budgets FAILED: ${message}\n`);
  process.exit(1);
}
