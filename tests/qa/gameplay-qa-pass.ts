import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import {
  GAMEPLAY_CAPABILITY_CATALOG,
  GAMEPLAY_HIDDEN_ROUTE_DISPOSITIONS,
  GAMEPLAY_SOURCE_RECORDS,
} from '../../src/tools/catalog/capabilities/records/gameplay/index.js';

const EXPECTED_COUNT = 356;

function fail(message: string): never {
  throw new TypeError(`GAMEPLAY_QA_FAIL ${message}`);
}

if (GAMEPLAY_SOURCE_RECORDS.length !== EXPECTED_COUNT) {
  fail(`source count ${GAMEPLAY_SOURCE_RECORDS.length}`);
}
if (GAMEPLAY_CAPABILITY_CATALOG.length !== EXPECTED_COUNT) {
  fail(`aggregate count ${GAMEPLAY_CAPABILITY_CATALOG.length}`);
}
if (new Set(GAMEPLAY_SOURCE_RECORDS.map((record) => record.id)).size !== EXPECTED_COUNT) {
  fail('canonical IDs are not unique');
}
if (!Object.isFrozen(GAMEPLAY_CAPABILITY_CATALOG)) {
  fail('aggregate is mutable');
}

const expectedHiddenCounts = { skeleton: 16, gas: 4, ai: 3 } as const;
for (const domain of Object.keys(expectedHiddenCounts)) {
  if (domain !== 'skeleton' && domain !== 'gas' && domain !== 'ai') {
    fail(`unexpected hidden-route domain ${domain}`);
  }
  const dispositions = GAMEPLAY_HIDDEN_ROUTE_DISPOSITIONS[domain];
  if (dispositions.length !== expectedHiddenCounts[domain]) {
    fail(`${domain} hidden-route count ${dispositions.length}`);
  }
  for (const disposition of dispositions) {
    const source = readFileSync(resolve(process.cwd(), disposition.evidence.source), 'utf8');
    if (!source.includes(disposition.evidence.symbol)) {
      fail(`${disposition.key} leaf symbol is absent from ${disposition.evidence.source}`);
    }
  }
}

process.stdout.write(`GAMEPLAY_QA_PASS ${EXPECTED_COUNT}\n`);
