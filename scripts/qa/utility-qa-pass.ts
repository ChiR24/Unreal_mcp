import { createHash } from 'node:crypto';
import { MANAGE_AUDIO_RECORDS } from '../../src/tools/catalog/capabilities/records/manage-audio/index.js';
import {
  MANAGE_NETWORKING_RECORDS,
  NETWORKING_PARTITION_COUNTS,
} from '../../src/tools/catalog/capabilities/records/manage-networking/index.js';
import { MANAGE_SEQUENCE_RECORDS } from '../../src/tools/catalog/capabilities/records/manage-sequence/index.js';
import {
  UTILITY_CAPABILITY_CATALOG,
  UTILITY_SOURCE_RECORDS,
} from '../../src/tools/catalog/capabilities/records/utility/index.js';

function assert(condition: boolean, message: string): void {
  if (!condition) {
    console.error(`UTILITY_QA_FAIL: ${message}`);
    process.exit(1);
  }
}

const sequenceIdentity = MANAGE_SEQUENCE_RECORDS.map(
  (record) => `${record.id}|${record.hashes.schema}|${record.hashes.content}`,
).join('\n');
assert(
  createHash('sha256').update(sequenceIdentity).digest('hex')
    === '3c91ab859e81a2b593179eeef71d88be23500188f08b275ef8ce88808465846e',
  'sequence baseline identity hash drifted',
);

assert(MANAGE_AUDIO_RECORDS.length === 50, 'manage_audio must contain 50 records');
assert(MANAGE_NETWORKING_RECORDS.length === 77, 'manage_networking must contain 77 records');
assert(
  JSON.stringify(NETWORKING_PARTITION_COUNTS)
    === JSON.stringify({ replication: 27, session: 16, gameFramework: 20, input: 14 }),
  'networking partition must be 27+16+20+14',
);
assert(UTILITY_CAPABILITY_CATALOG.length === 208, 'utility aggregate must contain 208 records');
assert(
  new Set(UTILITY_CAPABILITY_CATALOG.map((record) => record.id)).size === 208,
  'utility aggregate IDs must be unique',
);
assert(Object.isFrozen(UTILITY_CAPABILITY_CATALOG), 'utility aggregate must be frozen');
const utilityHashBody = UTILITY_CAPABILITY_CATALOG.map(
  (record) => `${record.id}|${record.hashes.schema}|${record.hashes.content}`,
).join('\n');
assert(
  createHash('sha256').update(utilityHashBody).digest('hex')
    === '48262ba08f85792cad4b6136dc8501cb4f506ef723008c00b098422809bd5f8a',
  'utility canonical hash drifted',
);

for (const source of MANAGE_SEQUENCE_RECORDS) {
  assert(
    UTILITY_SOURCE_RECORDS.find((record) => record.id === source.id) === source,
    `${source.id} must be reused by object identity`,
  );
}

const negativePlugins = new Set<string>();
for (const record of UTILITY_CAPABILITY_CATALOG) {
  if (record.availability.requiredPlugins.length === 0) continue;
  assert(
    !record.availability.requiredPlugins.every((plugin) => negativePlugins.has(plugin)),
    `${record.id} must fail closed without required plugins`,
  );
}

for (const record of [...MANAGE_AUDIO_RECORDS, ...MANAGE_NETWORKING_RECORDS]) {
  assert(!record.behavior.longRunning, `${record.id} must not claim asynchronous completion`);
}

console.log('UTILITY_QA_PASS 208');
