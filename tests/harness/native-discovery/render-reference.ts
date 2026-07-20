/// <reference types="node" />

// Renders the TypeScript discovery reference for every shared fixture case.
// The native harness renders the same cases from the generated shards; the two
// outputs are diffed byte-for-byte.

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { renderDiscovery } from '../../unit/plugin/gateway/native-discovery-reference.js';

interface FixtureCase {
  readonly name: string;
  readonly operation: string;
  readonly query?: string;
  readonly domain?: string;
  readonly family?: string;
  readonly tool?: string;
  readonly action?: string;
  readonly param?: string;
  readonly limit?: number;
  readonly offset?: number;
}

const casesPath = process.argv[2] ?? resolve(process.cwd(), 'tests/harness/native-discovery/cases.json');
const cases = JSON.parse(readFileSync(casesPath, 'utf8')) as readonly FixtureCase[];

for (const testCase of cases) {
  process.stdout.write(`${renderDiscovery(testCase)}\n`);
}
