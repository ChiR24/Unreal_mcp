/**
 * Gameplay record builder: buildCoreRecord with the gameplay conventions (an
 * explicit id, the parent tool as the discovery domain, and an input schema
 * without the `action` discriminator).
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildCoreRecord, type CoreRecordSpec } from '../core/builder.js';

export type RecordSpec = Omit<CoreRecordSpec, 'domain' | 'bareInput' | 'costLatency' | 'costResources' | 'required' | 'id'> & {
  readonly id: string;
  readonly required: readonly string[];
  readonly latency: NonNullable<CoreRecordSpec['costLatency']>;
  readonly resources: NonNullable<CoreRecordSpec['costResources']>;
};

export const buildRecord = ({ latency, resources, ...spec }: RecordSpec): CapabilityRecordSource =>
  buildCoreRecord({ ...spec, domain: spec.parentTool.replace(/_/g, ' '), bareInput: true, costLatency: latency, costResources: resources });
