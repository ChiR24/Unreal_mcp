// scripts/gateway-manifest/pilot.ts
// Pilot-only emitter: accepts validated CapabilityRecord[] and produces
// two deterministic outputs (neutral JSON and typed TS text). Does NOT alter
// production tools/list or any production artifact.
// Pilot files are not runtime imports - they are inspection artifacts.
// Output goes to an isolated pilot directory, never repo-root and never
// production paths.

import type { GatewayManifest, GatewayManifestTool } from '../../src/gateway/gateway-manifest-types.js';
import type { CapabilityRecord } from '../../src/tools/catalog/capabilities/model.js';
import { sortById } from '../../src/utils/serialization/ordering.js';

function recordParameterNames(record: CapabilityRecord): string[] {
  return Object.keys(record.schemas.input.properties)
    .filter((n) => n !== 'action' && n !== 'subAction' && n !== 'params')
    .sort();
}

function recordToManifestTool(record: CapabilityRecord): GatewayManifestTool {
  return {
    name: record.id,
    category: null,
    description: record.discovery.summary,
    actions: [record.routing.dispatchAction],
    parameterNames: recordParameterNames(record),
    inputSchema: record.schemas.input,
    perActionSchemas: false,
  };
}

export function buildPilotManifest(records: readonly CapabilityRecord[]): GatewayManifest {
  const sorted = sortById(records);
  return {
    version: 1,
    source: 'pilot:capabilityRecords',
    tools: sorted.map(recordToManifestTool),
  };
}

const PILOT_TS_HEADER = `/* eslint-disable */
// PILOT manifest - not a runtime import. Generated from canonical CapabilityRecord[].
// Do not import this file; it is an inspection artifact for pilot validation.
export const pilotManifest = `;

export function pilotJson(records: readonly CapabilityRecord[]): string {
  return `${JSON.stringify(buildPilotManifest(records), null, 2)}\n`;
}

export function pilotTsText(records: readonly CapabilityRecord[]): string {
  return `${PILOT_TS_HEADER}${JSON.stringify(buildPilotManifest(records), null, 2)};\n`;
}
