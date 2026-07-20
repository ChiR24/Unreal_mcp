import { createCapabilityRecord, type CapabilityRecord, type CapabilityRecordSource } from '../../index.js';
import { NETWORKING_FRAMEWORK_RECORDS } from './framework.data.js';
import { NETWORKING_INPUT_RECORDS } from './input.data.js';
import { NETWORKING_REPLICATION_RECORDS } from './replication.data.js';
import { NETWORKING_SESSION_RECORDS } from './session.data.js';

export const NETWORKING_PARTITION_COUNTS = Object.freeze({
  replication: NETWORKING_REPLICATION_RECORDS.length,
  session: NETWORKING_SESSION_RECORDS.length,
  gameFramework: NETWORKING_FRAMEWORK_RECORDS.length,
  input: NETWORKING_INPUT_RECORDS.length,
});

export const MANAGE_NETWORKING_SOURCES: readonly CapabilityRecordSource[] = Object.freeze([
  ...NETWORKING_REPLICATION_RECORDS,
  ...NETWORKING_SESSION_RECORDS,
  ...NETWORKING_FRAMEWORK_RECORDS,
  ...NETWORKING_INPUT_RECORDS,
].sort((left, right) => left.id.localeCompare(right.id)));

export const MANAGE_NETWORKING_RECORDS: readonly CapabilityRecord[] = Object.freeze(
  [...MANAGE_NETWORKING_SOURCES.map((source) => createCapabilityRecord(source))]
    .sort((left, right) => left.id.localeCompare(right.id)),
);

export const MANAGE_NETWORKING_RECORD_COUNT = MANAGE_NETWORKING_RECORDS.length;
