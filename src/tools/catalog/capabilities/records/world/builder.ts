/**
 * World-domain record builder (manage_level_structure, manage_geometry,
 * manage_pcg): buildCoreRecord under the `world` discovery domain, plus the two
 * things only world records use — folded legacy names and a dispatchBy route.
 */
import type { CapabilityRecordSource, JsonObject } from '../../model.js';
import { LegacyActionNameSchema } from '../../identifiers.js';
import { buildCoreRecord, type CoreRecordSpec } from '../core/builder.js';

export type FoldedActionSpec = {
  readonly action: string;
  readonly pins: JsonObject;
};

export type WorldRecordSpec = Omit<CoreRecordSpec, 'parentTool' | 'domain' | 'bareInput' | 'id' | 'required' | 'costLatency' | 'costResources'> & {
  readonly parentTool: 'manage_level_structure' | 'manage_geometry' | 'manage_pcg';
  readonly required: readonly string[];
  readonly costLatency: NonNullable<CoreRecordSpec['costLatency']>;
  readonly costResources: NonNullable<CoreRecordSpec['costResources']>;
  /**
   * Old action names this record replaced. Each stays callable by its own
   * name: the pins are the selector values that name implied, injected before
   * validation, and the old action is what the bridge receives.
   */
  readonly folded?: readonly FoldedActionSpec[];
  /** Selector value -> bridge action, for a call that names this record's own action. */
  readonly dispatchBy?: { readonly param: string; readonly actions: Readonly<Record<string, string>> };
};

export function buildWorldRecord({ folded, dispatchBy, ...spec }: WorldRecordSpec): CapabilityRecordSource {
  const record = buildCoreRecord({ ...spec, domain: 'world' });
  const tool = record.routing.parentTool;
  return {
    ...record,
    legacyIds: [
      ...record.legacyIds,
      ...(folded ?? []).map((entry) => ({ tool, action: LegacyActionNameSchema.parse(entry.action), folded: entry.pins })),
    ],
    routing: dispatchBy === undefined ? record.routing : {
      ...record.routing,
      dispatchBy: {
        param: dispatchBy.param,
        actions: Object.fromEntries(Object.entries(dispatchBy.actions).map(([value, action]) => [value, LegacyActionNameSchema.parse(action)])),
      },
    },
  };
}
