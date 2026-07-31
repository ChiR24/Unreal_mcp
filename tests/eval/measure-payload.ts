// tests/eval/measure-payload.ts
// Disclosure-economy measurement: how many bytes a client actually receives
// from `search` and `describe`, and how that compares to the union dump the
// gateway replaced.
//
// The union baseline is a counterfactual built from the SAME describe envelope
// with only the schema projection swapped for the parent tool's union
// inputSchema. Holding identity, policy and hashes constant is what makes the
// ratio measure progressive disclosure rather than incidental envelope size.

import { describeGatewayCapability } from '../../src/server/gateway/gateway-describe.js';
import { searchGatewayCapabilities } from '../../src/server/gateway/gateway-search.js';
import { findTool } from '../../src/server/gateway/gateway-shared.js';
import { isRecord } from '../../src/utils/validation/type-guards.js';
import type { CapabilityRecord } from '../../src/tools/catalog/capabilities/model.js';
import { corpus } from './corpus.js';
import { finalRegistryRecords, median } from './fixtures.js';

export function jsonBytes(value: unknown): number {
  return Buffer.byteLength(JSON.stringify(value) ?? '', 'utf8');
}

export type DescribeSample = {
  readonly capability: string;
  readonly describeBytes: number;
  readonly unionBaselineBytes: number;
  readonly ratio: number;
};

export type PayloadMeasurement = {
  readonly searchResponses: number;
  readonly maxSearchBytes: number;
  readonly medianSearchBytes: number;
  readonly describeResponses: number;
  readonly maxDescribeBytes: number;
  readonly medianDescribeBytes: number;
  readonly medianUnionBaselineBytes: number;
  readonly medianDescribeUnionRatio: number;
  readonly maxDescribeUnionRatio: number;
  readonly largestDescribeCapability: string | null;
  readonly unionComparableCapabilities: number;
};

/** Every distinct default-shaped `search` a corpus intent produces, plus browse. */
export function measureSearchBytes(): readonly number[] {
  const bytes: number[] = [];
  for (const entry of corpus.cases) {
    bytes.push(jsonBytes(searchGatewayCapabilities({ operation: 'search', query: entry.intent })));
  }
  bytes.push(jsonBytes(searchGatewayCapabilities({ operation: 'search' })));
  return Object.freeze(bytes);
}

function unionParameterSummaries(unionSchema: Record<string, unknown>): Array<Record<string, unknown>> {
  const properties = isRecord(unionSchema.properties) ? unionSchema.properties : {};
  const required = new Set(
    Array.isArray(unionSchema.required)
      ? unionSchema.required.filter((name): name is string => typeof name === 'string')
      : [],
  );
  return Object.keys(properties)
    .sort()
    .map((name) => {
      const schema = isRecord(properties[name]) ? (properties[name] as Record<string, unknown>) : {};
      const summary: Record<string, unknown> = {
        name,
        type: typeof schema.type === 'string' ? schema.type : 'unknown',
        required: required.has(name),
      };
      if (typeof schema.description === 'string') summary.description = schema.description;
      if (Array.isArray(schema.enum)) summary.enum = schema.enum;
      return summary;
    });
}

/**
 * What this capability's describe would weigh if it disclosed the parent tool's
 * union schema instead of the action's exact one. Returns undefined when the
 * parent tool is absent from the manifest, so a missing baseline is never
 * silently scored as a favourable ratio.
 */
export function unionBaselineBytes(
  record: CapabilityRecord,
  actual: Record<string, unknown>,
): number | undefined {
  const tool = findTool(record.routing.parentTool);
  if (tool === undefined) return undefined;
  const parameters = unionParameterSummaries(tool.inputSchema);
  return jsonBytes({
    ...actual,
    inputSchema: tool.inputSchema,
    parameters,
    parameterCount: parameters.length,
  });
}

export function describeSamples(): readonly DescribeSample[] {
  const samples: DescribeSample[] = [];
  for (const record of finalRegistryRecords()) {
    const actual = describeGatewayCapability({ operation: 'describe', capability: record.id });
    const describeBytes = jsonBytes(actual);
    const union = unionBaselineBytes(record, actual);
    if (union === undefined || union <= 0) continue;
    samples.push({
      capability: record.id,
      describeBytes,
      unionBaselineBytes: union,
      ratio: describeBytes / union,
    });
  }
  return Object.freeze(samples);
}

export function measurePayload(): PayloadMeasurement {
  const searchBytes = measureSearchBytes();
  const samples = describeSamples();
  const describeBytes = samples.map((sample) => sample.describeBytes);
  const largest = samples.reduce<DescribeSample | null>(
    (best, sample) => (best === null || sample.describeBytes > best.describeBytes ? sample : best),
    null,
  );
  return {
    searchResponses: searchBytes.length,
    maxSearchBytes: searchBytes.reduce((max, value) => Math.max(max, value), 0),
    medianSearchBytes: median(searchBytes),
    describeResponses: samples.length,
    maxDescribeBytes: describeBytes.reduce((max, value) => Math.max(max, value), 0),
    medianDescribeBytes: median(describeBytes),
    medianUnionBaselineBytes: median(samples.map((sample) => sample.unionBaselineBytes)),
    medianDescribeUnionRatio: median(samples.map((sample) => sample.ratio)),
    maxDescribeUnionRatio: samples.reduce((max, sample) => Math.max(max, sample.ratio), 0),
    largestDescribeCapability: largest === null ? null : largest.capability,
    unionComparableCapabilities: samples.length,
  };
}
