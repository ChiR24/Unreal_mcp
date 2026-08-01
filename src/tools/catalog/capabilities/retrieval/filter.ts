import type { CapabilityRecord } from '../model.js';
import { compareUnrealVersion } from '../version.js';
import type { CapabilityCategory, CapabilityRuntimeProfile } from './types.js';
import { compareById } from '../../../../utils/serialization/ordering.js';

export const PILOT_PARENT_CATEGORIES: Readonly<Record<string, CapabilityCategory>> =
  Object.freeze({
    build_environment: 'world',
    manage_asset: 'core',
    manage_blueprint: 'core',
    manage_sequence: 'utility',
  });

function hasRequiredVersion(
  record: CapabilityRecord,
  profile: CapabilityRuntimeProfile,
): boolean {
  return compareUnrealVersion(profile.unrealVersion, record.availability.unreal.min) >= 0
    && compareUnrealVersion(profile.unrealVersion, record.availability.unreal.max) <= 0;
}

function hasRequiredPlugins(
  record: CapabilityRecord,
  profile: CapabilityRuntimeProfile,
): boolean {
  const installed = new Set(profile.installedPlugins);
  return record.availability.requiredPlugins.every((plugin) => installed.has(plugin));
}

function hasEnabledRoute(
  record: CapabilityRecord,
  profile: CapabilityRuntimeProfile,
): boolean {
  const parent = record.routing.parentTool;
  const category = PILOT_PARENT_CATEGORIES[parent];
  return category !== undefined
    && profile.enabledParents.includes(parent)
    && profile.enabledCategories.includes(category);
}

function hasAuthorization(
  record: CapabilityRecord,
  profile: CapabilityRuntimeProfile,
): boolean {
  return profile.authorizedScopes.includes('admin')
    || profile.authorizedScopes.includes(record.policy.requiredScope);
}

function hasRequestedOutputs(
  record: CapabilityRecord,
  profile: CapabilityRuntimeProfile,
): boolean {
  return profile.requiredOutputFields.every((field) =>
    Object.hasOwn(record.schemas.output.properties, field),
  );
}

export function isCapabilityAvailable(
  record: CapabilityRecord,
  profile: CapabilityRuntimeProfile,
): boolean {
  return record.deprecation.status !== 'removed'
    && hasRequiredVersion(record, profile)
    && hasRequiredPlugins(record, profile)
    && record.availability.editorStates.includes(profile.editorState)
    && hasEnabledRoute(record, profile)
    && hasAuthorization(record, profile)
    && profile.requestedEffects.includes(record.behavior.effect)
    && hasRequestedOutputs(record, profile);
}

export function filterCapabilityRecords(
  records: readonly CapabilityRecord[],
  profile: CapabilityRuntimeProfile,
): readonly CapabilityRecord[] {
  return records
    .filter((record) => isCapabilityAvailable(record, profile))
    .sort(compareById);
}
