// src/tools/catalog/capabilities/records/shared/fold-spec.ts
//
// The fold vocabulary: a spec describes how sibling records that differ only by
// one selector value become a single record. See fold.ts for the transform.

export type { FoldSpec } from './fold-types.js';

/** Selector values that are the old action names themselves, for families whose verbs differ. */
export const byName = (actions: readonly string[]): Readonly<Record<string, string>> =>
  Object.fromEntries(actions.map((action) => [action, action]));

/** Selector values derived by stripping a shared prefix (and optional suffix) from each old action. */
export function byTarget(prefix: string, actions: readonly string[], suffix = ''): Readonly<Record<string, string>> {
  return Object.fromEntries(actions.map((action) => {
    if (!action.startsWith(prefix) || !action.endsWith(suffix) || action.length <= prefix.length + suffix.length) {
      throw new Error(`byTarget: '${action}' does not carry the prefix '${prefix}'${suffix === '' ? '' : ` and suffix '${suffix}'`}`);
    }
    return [action.slice(prefix.length, action.length - suffix.length), action];
  }));
}
