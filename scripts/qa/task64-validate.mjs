// scripts/qa/task64-validate.mjs
//
// Runs the todo 50 evidence validator against the todo 64 readiness record and
// prints its verdict verbatim. A record that cannot survive the project's own
// validator is not evidence.

import { readFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

import { describeRejections, validateEvidence } from '../../tests/unit/task-50/evidence-validator.mjs';

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const target = process.argv[2] ?? '.omo/evidence/task-64-pure-unreal-mcp-implementation.json';
const document = JSON.parse(readFileSync(join(ROOT, target), 'utf8'));

const result = validateEvidence(document, { projectRoot: ROOT });

process.stdout.write(`target: ${target}\n`);
process.stdout.write(`valid: ${result.valid}\n`);
process.stdout.write(`checked: ${JSON.stringify(result.checked)}\n`);
process.stdout.write(`${describeRejections(result)}\n`);

process.exitCode = result.valid ? 0 : 1;
