// src/server/gateway/gateway-availability.ts
// Whether a canonical capability may be presented as runnable, and why not.
//
// Discovery must never advertise something the caller cannot actually invoke,
// so availability is computed from live session state (the dynamic tool
// manager) plus the record's own deprecation status. The declared environment
// requirements (engine version, plugins, editor states) are reported as data
// rather than decided here: the TypeScript surface has no live editor to probe
// at discovery time, and guessing would produce a confident wrong answer.

import { dynamicToolManager } from '../../tools/dynamic/dynamic-tool-manager.js';
import type { CapabilityRecord } from '../../tools/catalog/capabilities/model.js';
import { compareAscii } from '../../utils/serialization/ordering.js';

export type AvailabilityStatus = 'available' | 'disabled' | 'unavailable';

export type CapabilityAvailability = {
  readonly status: AvailabilityStatus;
  readonly reasons: readonly string[];
  readonly unreal: { readonly min: unknown; readonly max: unknown };
  readonly requiredPlugins: readonly string[];
  readonly editorStates: readonly string[];
};

function statusReasons(record: CapabilityRecord): readonly string[] {
  const reasons: string[] = [];
  if (record.deprecation.status === 'removed') reasons.push('capability_removed');
  if (record.deprecation.status === 'deprecated') reasons.push('capability_deprecated');
  if (!dynamicToolManager.isToolEnabled(record.routing.parentTool)) {
    reasons.push('parent_tool_disabled');
  }
  return reasons;
}

function statusFor(reasons: readonly string[]): AvailabilityStatus {
  if (reasons.includes('capability_removed')) return 'unavailable';
  if (reasons.includes('parent_tool_disabled')) return 'disabled';
  return 'available';
}

export function capabilityAvailability(record: CapabilityRecord): CapabilityAvailability {
  const reasons = statusReasons(record);
  return {
    status: statusFor(reasons),
    reasons,
    unreal: { min: record.availability.unreal.min, max: record.availability.unreal.max },
    requiredPlugins: [...record.availability.requiredPlugins].sort(compareAscii),
    editorStates: [...record.availability.editorStates].sort(compareAscii)
  };
}

export function isRunnable(availability: CapabilityAvailability): boolean {
  return availability.status === 'available';
}
