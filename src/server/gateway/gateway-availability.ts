// src/server/gateway/gateway-availability.ts
// Whether a canonical capability may be presented as runnable, and why not.
//
// Discovery must never advertise something the caller cannot invoke, so the
// status follows live session state (the dynamic tool manager). The declared
// environment requirements (plugins, editor states) are reported as data rather
// than decided here: the TypeScript surface has no live editor to probe at
// discovery time, and guessing would produce a confident wrong answer.

import { dynamicToolManager } from '../../tools/dynamic/dynamic-tool-manager.js';
import type { CapabilityRecord } from '../../tools/catalog/capabilities/model.js';
import { compareAscii } from '../../utils/serialization/ordering.js';

export type CapabilityAvailability = {
  readonly status: 'available' | 'disabled';
  readonly reasons: readonly string[];
  readonly requiredPlugins: readonly string[];
  readonly editorStates: readonly string[];
};

export function capabilityAvailability(record: CapabilityRecord): CapabilityAvailability {
  const enabled = dynamicToolManager.isToolEnabled(record.routing.parentTool);
  return {
    status: enabled ? 'available' : 'disabled',
    reasons: enabled ? [] : ['parent_tool_disabled'],
    requiredPlugins: [...record.availability.requiredPlugins].sort(compareAscii),
    editorStates: [...record.availability.editorStates].sort(compareAscii)
  };
}

export function isRunnable(availability: CapabilityAvailability): boolean {
  return availability.status === 'available';
}
