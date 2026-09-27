// src/tools/catalog/capabilities/records/shared/fold-members.ts
//
// Turns a fold spec into the ordered member entries the transform consumes, and
// refuses a fold whose members disagree on the facets a caller relies on
// (effect, policy, availability, dispatch mode, family, id namespace), so
// folding can never widen a family's own policy.

import type { CapabilityRecordSource } from '../../model.js';
import type { FoldSpec, MemberEntry } from './fold-types.js';
import { sameJson, unique } from './fold-support.js';

const isStringList = (value: unknown): value is readonly string[] => Array.isArray(value);

const actionOf = (record: CapabilityRecordSource): string => String(record.legacyIds[0]?.action ?? '');
const namespaceOf = (id: string): string => id.slice(0, id.lastIndexOf('.'));

export function entriesOf(spec: FoldSpec): readonly MemberEntry[] {
  const entries: MemberEntry[] = [];
  if (isStringList(spec.members)) {
    if (spec.selector !== undefined) throw new Error(`fold ${spec.primary}: a selector needs value -> action members`);
    // An alias fold keeps the primary's own record; the listed names are its aliases.
    entries.push(
      { value: undefined, action: spec.primary },
      ...spec.members.map((action) => ({ value: undefined, action })),
    );
  } else {
    if (spec.selector === undefined) throw new Error(`fold ${spec.primary}: value -> action members need a selector`);
    entries.push(...Object.entries(spec.members).map(([value, action]) => ({ value, action, pin: value })));
  }
  const aliasMembers = spec.aliasMembers ?? [];
  if (isStringList(aliasMembers)) {
    entries.push(...aliasMembers.map((action) => ({ value: undefined, action })));
  } else {
    // A self-dispatching alias under a REQUIRED selector must pin the value its
    // name implied, or a call by that name cannot satisfy validation.
    entries.push(...Object.entries(aliasMembers).flatMap(([value, actions]) =>
      [actions].flat().map((action) => ({ value: undefined, action, pin: value }))));
  }
  if (unique(entries.map((entry) => entry.action)).length !== entries.length) {
    throw new Error(`fold ${spec.primary}: a member action is listed twice`);
  }
  if (spec.selector !== undefined && entries.some((entry) => entry.action === spec.primary && entry.pin === undefined)) {
    throw new Error(`fold ${spec.primary}: the primary of a selector fold must carry a selector value`);
  }
  return entries;
}

export function assertFoldable(spec: FoldSpec, members: readonly CapabilityRecordSource[], parentTool: string): void {
  const first = members[0];
  if (first === undefined) throw new Error(`fold ${spec.primary}: no members`);
  for (const member of members) {
    const action = actionOf(member);
    if (member.routing.dispatchBy !== undefined) throw new Error(`fold ${spec.primary}: member ${action} is itself a fold`);
    if (member.legacyIds.length !== 1) throw new Error(`fold ${spec.primary}: member ${action} carries folded pairs already`);
    if (String(member.routing.parentTool) !== parentTool) throw new Error(`fold ${spec.primary}: member ${action} belongs to ${String(member.routing.parentTool)}`);
    if (spec.selector !== undefined && Object.hasOwn(member.schemas.input.properties, spec.selector)) {
      throw new Error(`fold ${spec.primary}: member ${action} already declares a '${spec.selector}' parameter; pick another selector name`);
    }
    const facets: ReadonlyArray<readonly [string, unknown, unknown]> = [
      ['behavior.effect', first.behavior.effect, member.behavior.effect],
      ['policy', first.policy, member.policy],
      ['availability', first.availability, member.availability],
      ['discovery.family', first.discovery.family, member.discovery.family],
      ['discovery.domain', first.discovery.domain, member.discovery.domain],
      ['id namespace', namespaceOf(String(first.id)), namespaceOf(String(member.id))],
      ['routing.parentTool', first.routing.parentTool, member.routing.parentTool],
    ];
    for (const [facet, expected, actual] of facets) {
      if (!sameJson(expected, actual)) {
        throw new Error(`fold ${spec.primary}: member ${action} differs from ${actionOf(first)} on ${facet}`);
      }
    }
  }
}
