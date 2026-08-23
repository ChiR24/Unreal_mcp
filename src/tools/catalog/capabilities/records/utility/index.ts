import type { CapabilityCatalog, CapabilityRecord } from '../../index.js';
import { parseCapabilityCatalog } from '../../index.js';
import { MANAGE_AUDIO_RECORDS } from '../manage-audio/index.js';
import { MANAGE_NETWORKING_RECORDS } from '../manage-networking/index.js';
import { MANAGE_SEQUENCE_RECORDS } from '../manage-sequence/index.js';
import { compareById } from '../../../../../utils/serialization/ordering.js';

export const UTILITY_NET_NEW_COUNT = 128 as const;
export const UTILITY_REUSED_SEQUENCE_COUNT = 81 as const;
export const UTILITY_AGGREGATE_COUNT = 209 as const;

const compareIds = compareById;

export const UTILITY_SOURCE_RECORDS: readonly CapabilityRecord[] = Object.freeze([
  ...MANAGE_SEQUENCE_RECORDS,
  ...MANAGE_AUDIO_RECORDS,
  ...MANAGE_NETWORKING_RECORDS,
]);

const parsed = parseCapabilityCatalog([...UTILITY_SOURCE_RECORDS]);
const uniqueIds = new Set(parsed.map((record) => record.id)).size;
if (parsed.length !== UTILITY_AGGREGATE_COUNT || uniqueIds !== UTILITY_AGGREGATE_COUNT) {
  throw new TypeError(
    `Utility capability aggregate must contain exactly ${UTILITY_AGGREGATE_COUNT} unique records; `
    + `received ${parsed.length} records and ${uniqueIds} unique IDs`,
  );
}

export const UTILITY_CAPABILITY_CATALOG: CapabilityCatalog = Object.freeze(
  [...parsed].sort(compareIds),
);
export const UTILITY_CAPABILITY_RECORDS = UTILITY_CAPABILITY_CATALOG;
export const UTILITY_CAPABILITY_RECORD_COUNT = UTILITY_CAPABILITY_CATALOG.length;
