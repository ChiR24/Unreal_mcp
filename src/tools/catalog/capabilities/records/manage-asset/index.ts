// Aggregates all 167 manage_asset capability record specs across asset core,
// material, texture, struct, DataTable, and enum families, validates them via
// createCapabilityRecord, and exports the hashed CapabilityRecord[].
import { createCapabilityRecord } from '../../index.js';
import type { CapabilityRecord } from '../../model.js';
import { ASSET_ADVANCED_RECORDS } from './asset-advanced.js';
import { ASSET_LIFECYCLE_RECORDS } from './asset-lifecycle.js';
import { ASSET_QUERY_RECORDS } from './asset-query.js';
import type { RecordSpec } from './builder.js';
import { toSource } from './builder.js';
import { CONTENT_SOURCE_RECORDS } from './content-sources.js';
import { DATATABLE_RECORDS } from './datatable-records.js';
import { ENUM_RECORDS } from './enum-records.js';
import { MATERIAL_CREATE_RECORDS } from './material-create.js';
import { MATERIAL_GRAPH_RECORDS } from './material-graph.js';
import { MATERIAL_NODES_RECORDS } from './material-nodes.js';
import { MATERIAL_PARAMS_RECORDS } from './material-params.js';
import { STRUCT_RECORDS } from './struct-records.js';
import { TEXTURE_ADJUST_RECORDS } from './texture-adjust.js';
import { TEXTURE_CONFIG_RECORDS } from './texture-config.js';
import { TEXTURE_CREATE_RECORDS } from './texture-create.js';

export const MANAGE_ASSET_RECORD_SPECS: readonly RecordSpec[] = [
  ...ASSET_LIFECYCLE_RECORDS, ...CONTENT_SOURCE_RECORDS, ...ASSET_QUERY_RECORDS, ...ASSET_ADVANCED_RECORDS,
  ...MATERIAL_CREATE_RECORDS, ...MATERIAL_NODES_RECORDS, ...MATERIAL_GRAPH_RECORDS, ...MATERIAL_PARAMS_RECORDS,
  ...TEXTURE_CREATE_RECORDS, ...TEXTURE_ADJUST_RECORDS, ...TEXTURE_CONFIG_RECORDS,
  ...STRUCT_RECORDS, ...DATATABLE_RECORDS, ...ENUM_RECORDS
];

export const MANAGE_ASSET_RECORDS: readonly CapabilityRecord[] = MANAGE_ASSET_RECORD_SPECS.map(spec =>
  createCapabilityRecord(toSource(spec))
);

export const MANAGE_ASSET_RECORD_COUNT = MANAGE_ASSET_RECORDS.length;

export const MANAGE_ASSET_EXPECTED_IDS: readonly string[] = MANAGE_ASSET_RECORD_SPECS.map(spec =>
  `${spec.family}.${spec.action}`
);
