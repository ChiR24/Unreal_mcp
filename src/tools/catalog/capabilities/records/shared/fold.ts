// src/tools/catalog/capabilities/records/shared/fold.ts
//
// Apply the folds to a parent's source records, in spec order. Members are
// replaced by the folded record at the first member's position, so the
// parent's authored action order is preserved.
//
// Specs are data; the transform refuses a fold whose members disagree on the
// facets a caller relies on (see fold-members.ts) and merges the surviving
// schema with fold-widen.ts.

import type { CapabilityRecordSource } from '../../model.js';
import type { FoldSpec } from './fold-types.js';
import { assertFoldable, entriesOf } from './fold-members.js';
import { buildFolded } from './fold-build.js';

export type { FoldSpec, MemberEntry } from './fold-types.js';
export { byName, byTarget } from './fold-spec.js';

const actionOf = (record: CapabilityRecordSource): string => String(record.legacyIds[0]?.action ?? '');

export function applyFolds(
  records: readonly CapabilityRecordSource[],
  specs: readonly FoldSpec[],
  parentTool: string,
): readonly CapabilityRecordSource[] {
  let out: CapabilityRecordSource[] = [...records];
  for (const spec of specs) {
    const entries = entriesOf(spec);
    const index = new Map(out.map((record, position) => [actionOf(record), position] as const));
    const positions = entries.map((entry) => {
      const position = index.get(entry.action);
      if (position === undefined) throw new Error(`fold ${spec.primary} (${parentTool}): member '${entry.action}' not found`);
      return position;
    });
    if (!entries.some((entry) => entry.action === spec.primary) && index.has(spec.primary)) {
      throw new Error(`fold ${spec.primary} (${parentTool}): an unrelated record already uses that action`);
    }
    const members = positions.map((position) => out[position]).filter((record): record is CapabilityRecordSource => record !== undefined);
    assertFoldable(spec, members, parentTool);
    const folded = buildFolded(parentTool, spec, entries, members);
    const removed = new Set(positions);
    const at = Math.min(...positions);
    const next: CapabilityRecordSource[] = [];
    out.forEach((record, position) => {
      if (position === at) next.push(folded);
      if (!removed.has(position)) next.push(record);
    });
    out = next;
  }
  return out;
}
