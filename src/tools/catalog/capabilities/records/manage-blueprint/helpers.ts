/**
 * manage_blueprint record builder: buildCoreRecord with an explicit id and the
 * Blueprint plugin set as the default availability.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildCoreRecord, type CoreRecordSpec } from '../core/builder.js';

// UMG requires the UMG plugin; Blueprint core needs only EditorScriptingUtilities.
export const BP_PLUGINS = ['EditorScriptingUtilities'];
export const WIDGET_PLUGINS = ['EditorScriptingUtilities', 'UMG'];

export type RecordSpec = Omit<CoreRecordSpec, 'parentTool' | 'bareInput' | 'costLatency' | 'costResources' | 'id'> & {
  readonly id: string;
  readonly latency: NonNullable<CoreRecordSpec['costLatency']>;
  readonly resources: NonNullable<CoreRecordSpec['costResources']>;
};

export const buildRecord = ({ latency, resources, ...spec }: RecordSpec): CapabilityRecordSource =>
  buildCoreRecord({ ...spec, parentTool: 'manage_blueprint', plugins: spec.plugins ?? BP_PLUGINS, costLatency: latency, costResources: resources });
