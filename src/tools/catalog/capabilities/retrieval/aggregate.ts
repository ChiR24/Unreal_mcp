import type {
  CapabilityCatalog,
  CapabilityRecord,
  CapabilityRecordSource,
} from '../model.js';
import { createCapabilityRecord, parseCapabilityCatalog } from '../parser.js';
import { BUILD_ENVIRONMENT_RECORDS } from '../records/build-environment/index.js';
import { CONTROL_ACTOR_RECORDS } from '../records/control-actor/index.js';
import { CONTROL_EDITOR_RECORDS } from '../records/control-editor/index.js';
import { INSPECT_RECORDS } from '../records/inspect/index.js';
import { MANAGE_ASSET_RECORDS } from '../records/manage-asset/index.js';
import { MANAGE_BLUEPRINT_RECORDS } from '../records/manage-blueprint/index.js';
import { MANAGE_LEVEL_RECORDS } from '../records/manage-level/index.js';
import { MANAGE_SEQUENCE_RECORDS } from '../records/manage-sequence/index.js';
import { MANAGE_TOOLS_RECORDS } from '../records/manage-tools/index.js';
import { SYSTEM_CONTROL_RECORDS } from '../records/system-control/index.js';
import { compareById as compareCanonicalIds } from '../../../../utils/serialization/ordering.js';

export const PILOT_CAPABILITY_RECORD_COUNT = 493 as const;
export const CORE_CAPABILITY_RECORD_COUNT = 470 as const;

export type PilotCapabilityCatalogSources = {
  readonly buildEnvironment: readonly CapabilityRecordSource[];
  readonly manageAsset: readonly CapabilityRecord[];
  readonly manageBlueprint: readonly CapabilityRecord[];
  readonly manageSequence: readonly CapabilityRecord[];
};

export type CoreCapabilityCatalogSources = {
  readonly manageAsset: readonly CapabilityRecord[];
  readonly manageBlueprint: readonly CapabilityRecord[];
  readonly controlActor: readonly CapabilityRecord[];
  readonly controlEditor: readonly CapabilityRecord[];
  readonly manageLevel: readonly CapabilityRecord[];
  readonly systemControl: readonly CapabilityRecord[];
  readonly inspect: readonly CapabilityRecord[];
  readonly manageTools: readonly CapabilityRecord[];
};

type CapabilityCatalogSizeDetails = {
  readonly actualCount: number;
  readonly uniqueIdCount: number;
  readonly expectedCount: number;
  readonly catalogName: string;
};

export class CapabilityCatalogSizeError extends Error {
  readonly name = 'CapabilityCatalogSizeError';
  readonly actualCount: number;
  readonly uniqueIdCount: number;
  readonly expectedCount: number;
  readonly catalogName: string;

  constructor(details: CapabilityCatalogSizeDetails) {
    super(
      `${details.catalogName} capability catalog must contain exactly ${details.expectedCount} `
      + `records and unique IDs; received ${details.actualCount} records and ${details.uniqueIdCount} unique IDs`,
    );
    this.actualCount = details.actualCount;
    this.uniqueIdCount = details.uniqueIdCount;
    this.expectedCount = details.expectedCount;
    this.catalogName = details.catalogName;
  }
}

/**
 * The eight core parents in AUTHORED order — the sequence each record
 * directory declares. The canonical-registry generator derives every parent's
 * action enum from the first-seen record sequence, so it must consume this
 * view; the id-sorted catalog below would alphabetise those enums.
 */
export function createCoreCapabilitySourceView(
  sources: CoreCapabilityCatalogSources,
): CapabilityCatalog {
  const parsed = parseCapabilityCatalog([
    ...sources.manageAsset,
    ...sources.manageBlueprint,
    ...sources.controlActor,
    ...sources.controlEditor,
    ...sources.manageLevel,
    ...sources.systemControl,
    ...sources.inspect,
    ...sources.manageTools,
  ]);
  const uniqueIdCount = new Set(parsed.map((record) => record.id)).size;
  if (
    parsed.length !== CORE_CAPABILITY_RECORD_COUNT
    || uniqueIdCount !== CORE_CAPABILITY_RECORD_COUNT
  ) {
    throw new CapabilityCatalogSizeError({
      actualCount: parsed.length,
      uniqueIdCount,
      expectedCount: CORE_CAPABILITY_RECORD_COUNT,
      catalogName: 'Core',
    });
  }
  return Object.freeze(parsed);
}

/** Id-sorted retrieval projection of {@link createCoreCapabilitySourceView}. */
export function createCoreCapabilityCatalog(
  sources: CoreCapabilityCatalogSources,
): CapabilityCatalog {
  return Object.freeze(
    [...createCoreCapabilitySourceView(sources)].sort(compareCanonicalIds),
  );
}

export function createPilotCapabilityCatalog(
  sources: PilotCapabilityCatalogSources,
): CapabilityCatalog {
  const environmentRecords = sources.buildEnvironment.map((source) =>
    createCapabilityRecord(source),
  );
  const parsed = parseCapabilityCatalog([
    ...environmentRecords,
    ...sources.manageAsset,
    ...sources.manageBlueprint,
    ...sources.manageSequence,
  ]);
  const uniqueIdCount = new Set(parsed.map((record) => record.id)).size;
  if (
    parsed.length !== PILOT_CAPABILITY_RECORD_COUNT
    || uniqueIdCount !== PILOT_CAPABILITY_RECORD_COUNT
  ) {
    throw new CapabilityCatalogSizeError({
      actualCount: parsed.length,
      uniqueIdCount,
      expectedCount: PILOT_CAPABILITY_RECORD_COUNT,
      catalogName: 'Pilot',
    });
  }
  return Object.freeze([...parsed].sort(compareCanonicalIds));
}

const PILOT_CAPABILITY_SOURCES = Object.freeze({
  buildEnvironment: BUILD_ENVIRONMENT_RECORDS,
  manageAsset: MANAGE_ASSET_RECORDS,
  manageBlueprint: MANAGE_BLUEPRINT_RECORDS,
  manageSequence: MANAGE_SEQUENCE_RECORDS,
} satisfies PilotCapabilityCatalogSources);

export const PILOT_CAPABILITY_CATALOG = createPilotCapabilityCatalog(
  PILOT_CAPABILITY_SOURCES,
);

const CORE_CAPABILITY_SOURCES = Object.freeze({
  manageAsset: MANAGE_ASSET_RECORDS,
  manageBlueprint: MANAGE_BLUEPRINT_RECORDS,
  controlActor: CONTROL_ACTOR_RECORDS,
  controlEditor: CONTROL_EDITOR_RECORDS,
  manageLevel: MANAGE_LEVEL_RECORDS,
  systemControl: SYSTEM_CONTROL_RECORDS,
  inspect: INSPECT_RECORDS,
  manageTools: MANAGE_TOOLS_RECORDS,
} satisfies CoreCapabilityCatalogSources);

export const CORE_CAPABILITY_SOURCE_RECORDS: CapabilityCatalog = createCoreCapabilitySourceView(
  CORE_CAPABILITY_SOURCES,
);

export const CORE_CAPABILITY_CATALOG: CapabilityCatalog = Object.freeze(
  [...CORE_CAPABILITY_SOURCE_RECORDS].sort(compareCanonicalIds),
);
