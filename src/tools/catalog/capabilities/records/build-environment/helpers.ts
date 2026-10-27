/**
 * build_environment record builder: buildCoreRecord with an explicit id and the
 * `environment` discovery domain.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildCoreRecord, type CoreRecordSpec } from '../core/builder.js';

export type RecordSpec = Omit<CoreRecordSpec, 'parentTool' | 'domain' | 'bareInput' | 'costLatency' | 'costResources' | 'id'> & {
  readonly id: string;
  readonly latency: NonNullable<CoreRecordSpec['costLatency']>;
  readonly resources: NonNullable<CoreRecordSpec['costResources']>;
};

export const buildRecord = ({ latency, resources, ...spec }: RecordSpec): CapabilityRecordSource =>
  buildCoreRecord({ ...spec, parentTool: 'build_environment', domain: 'environment', costLatency: latency, costResources: resources });
