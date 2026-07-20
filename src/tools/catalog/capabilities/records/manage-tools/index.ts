// Aggregates the 8 manage_tools capability records in canonical action-enum
// order (list_tools, list_categories, enable_tools, disable_tools,
// enable_category, disable_category, get_status, reset), builds
// CapabilityRecordSource values via buildCoreRecord, validates them through
// createCapabilityRecord, and exports the hashed records plus the count
// consumed by the core capability aggregate.
import { createCapabilityRecord } from '../../index.js';
import type { CapabilityRecord, CapabilityRecordSource } from '../../model.js';
import { buildCoreRecord, type CoreRecordSpec } from '../core/builder.js';
import { MANAGE_TOOLS_READ_SPECS } from './records-read.js';
import { MANAGE_TOOLS_WRITE_SPECS } from './records-write.js';

const MANAGE_TOOLS_SPECS: readonly CoreRecordSpec[] = [
  MANAGE_TOOLS_READ_SPECS[0],  // list_tools
  MANAGE_TOOLS_READ_SPECS[1],  // list_categories
  MANAGE_TOOLS_WRITE_SPECS[0], // enable_tools
  MANAGE_TOOLS_WRITE_SPECS[1], // disable_tools
  MANAGE_TOOLS_WRITE_SPECS[2], // enable_category
  MANAGE_TOOLS_WRITE_SPECS[3], // disable_category
  MANAGE_TOOLS_READ_SPECS[2],  // get_status
  MANAGE_TOOLS_WRITE_SPECS[4], // reset
];

export const MANAGE_TOOLS_SOURCES: readonly CapabilityRecordSource[] =
  MANAGE_TOOLS_SPECS.map((spec) => buildCoreRecord(spec));

export const MANAGE_TOOLS_RECORDS: readonly CapabilityRecord[] =
  MANAGE_TOOLS_SOURCES.map((source) => createCapabilityRecord(source));

export const MANAGE_TOOLS_RECORD_COUNT = MANAGE_TOOLS_RECORDS.length;
