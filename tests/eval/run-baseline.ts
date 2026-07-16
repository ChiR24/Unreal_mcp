// tests/eval/run-baseline.ts
// Manual QA entrypoint for Task 4. Loads the gateway manifest, validates the
// golden corpus against it (fail-closed on drift or incomplete coverage),
// scores the corpus with the deterministic offline scorer, and writes a
// machine-readable baseline report. Exit code reflects the machine result:
// 0 only when 23-parent coverage, collision coverage, and all validations pass.

import { writeFileSync } from 'node:fs';
import { assertCollisionCoverage, assertFullParentCoverage, corpus, validateCorpus } from './corpus.js';
import { CorpusValidationError } from './errors.js';
import { loadManifestModel } from './manifest-model.js';
import { scoreCorpus } from './scorer.js';

function parseArgs(argv: readonly string[]): { manifest: string; output: string } {
  let manifest = 'src/gateway/gateway-manifest.generated.json';
  let output = '.omo/evidence/task-4-baseline.json';
  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === '--manifest') {
      const next = argv[i + 1];
      if (next === undefined) throw new Error('--manifest requires a path');
      manifest = next;
      i += 1;
    } else if (arg === '--output') {
      const next = argv[i + 1];
      if (next === undefined) throw new Error('--output requires a path');
      output = next;
      i += 1;
    }
  }
  return { manifest, output };
}

function main(): void {
  const { manifest: manifestPath, output: outputPath } = parseArgs(process.argv.slice(2));
  const manifest = loadManifestModel(manifestPath);
  validateCorpus(corpus, manifest);
  assertFullParentCoverage(corpus, manifest);
  const collisionCoverage = assertCollisionCoverage(corpus);
  const report = scoreCorpus(corpus, manifest);

  const payload = {
    ...report,
    task: 'Task 4 - golden discovery / model-call evaluation corpus + deterministic baseline scorer',
    generatedAt: new Date().toISOString(),
    collisionCoverageCount: collisionCoverage.length,
    parentCoverageCount: report.metrics.parentCoverage.length,
  };
  writeFileSync(outputPath, `${JSON.stringify(payload, null, 2)}\n`, 'utf8');

  const m = report.metrics;
  process.stdout.write(
    `Task4 baseline: corpusHash=${report.corpusHash.slice(0, 16)} ` +
      `parents=${m.parentCoverage.length} collisions=${collisionCoverage.length} ` +
      `top1=${(m.top1Accuracy * 100).toFixed(1)}% topK=${(m.topKAccuracy * 100).toFixed(1)}% ` +
      `disclosureBytes=${report.disclosure.totalBytes} tokens=${report.disclosure.totalTokens}\n`,
  );
}

try {
  main();
} catch (error) {
  if (error instanceof CorpusValidationError) {
    process.stderr.write(`Task4 baseline FAILED: ${error.message}\n`);
    process.exit(1);
  }
  if (error instanceof Error) {
    process.stderr.write(`Task4 baseline FAILED: ${error.message}\n`);
    process.exit(1);
  }
  process.stderr.write('Task4 baseline FAILED: unknown error\n');
  process.exit(1);
}
