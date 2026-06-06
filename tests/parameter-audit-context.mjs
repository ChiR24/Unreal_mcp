import path from 'node:path';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';

const auditFile = fileURLToPath(import.meta.url);
const testsDir = path.dirname(auditFile);

export const repoRoot = path.resolve(testsDir, '..');
export const definitionsRoot = path.join(repoRoot, 'src/tools/definitions');
export const testsRoot = path.join(repoRoot, 'tests/mcp-tools');
export const integrationSuitePath = path.join(repoRoot, 'tests/integration.mjs');
export const reportsDir = path.join(repoRoot, 'tests/reports');
export const requireFromAudit = createRequire(import.meta.url);
export const AsyncFunction = Object.getPrototypeOf(async function () {}).constructor;
