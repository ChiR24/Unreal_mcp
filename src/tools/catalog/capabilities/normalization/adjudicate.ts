/**
 * Adjudication tables for the A-F normalization taxonomy.
 *
 * These tables are the explicit, reviewed disposition data required by the
 * plan. They are DATA, not computed guesses: every entry carries a human
 * justification and is the single source of truth for how a duplicated or
 * special action is normalized. The generator never merges solely on
 * action-name equality; the merged/shared set below is the only place two
 * tools are allowed to collapse to one canonical id, and it is restricted to
 * capabilities that are genuinely identical across surfaces (editor/system
 * console + viewport controls). The four `delete` targets are deliberately
 * NOT in this set, so they remain distinct canonical capabilities.
 *
 * The prior 'P' (primary baseline) class has been removed. Primary/alias
 * position is now carried by the separate `role` field on each occurrence, so
 * the `classification` field stays strictly A-F for all 1,335 occurrences.
 */

import type { Classification, Disposition, Role } from './types.js';

/** True-duplicate action names that collapse to ONE shared canonical id. */
export const MERGED_SHARED_NAMES: ReadonlySet<string> = new Set([
  'console_command',
  'execute_command',
  'screenshot',
  'show_stats',
  'get_project_settings',
]);

/** Preset/workflow actions (UI scaffolds) classified E. */
const PRESET_PATTERN = /^create_.*(_menu|_screen|_widget|_ui)$/;

export function isPreset(action: string): boolean {
  return PRESET_PATTERN.test(action);
}

/** Canonical id for a true-duplicate (shared) capability. */
export function mergedCanonicalId(action: string): string {
  return `cap:shared:${action}`;
}

/** Canonical id for a target-namespaced (tool-scoped) capability. */
export function toolCanonicalId(tool: string, action: string): string {
  return `cap:${tool}:${action}`;
}

/** Semantic namespace encoded in a canonical id. */
export function namespaceOf(canonicalId: string): string {
  const parts = canonicalId.split(':');
  // Drop leading "cap" and trailing action segment.
  return parts.slice(1, parts.length - 1).join(':');
}

/**
 * Reviewed fixed metrics that must be reproduced exactly from source.
 *
 * Re-reviewed once, for `manage_level.set_world_settings`: the native Level
 * dispatch already routed `set_level_world_settings`, but no record published
 * it, so a level's GameMode override was unreachable through the gateway and
 * callers had to fall back to system_control.execute_python. Publishing it adds
 * exactly one occurrence (1335 -> 1336) and one `set`-family verb (817 -> 818).
 * It introduces no duplicate name — `set_world_settings` is unique across the
 * surface — so the three duplicate/reduction metrics are unchanged.
 *
 * Re-reviewed a second time for `material.set_node_position`: material nodes
 * could be created at a coordinate but never moved, so a badly laid-out graph
 * could only be fixed by removing and re-adding the node, which drops its
 * connections. Publishing the move adds exactly one occurrence (1336 -> 1337)
 * and one `set`-family verb (818 -> 820); `set_node_position` is unique across
 * the surface, so the duplicate/reduction metrics are again unchanged.
 *
 * Re-reviewed a third time for content ingestion — `asset.list_content_sources`,
 * `asset.migrate_assets`, `system_control.list_plugins`,
 * `system_control.enable_plugin`, `system_control.disable_plugin`. These are
 * authored after the gateway migration and marked `post-migration`, so
 * `extractOccurrences()` skips them and they do not move the audited total.
 *
 * Re-reviewed a fourth time for `asset.list_fab_downloads`. Also post-migration,
 * so the audited total is unaffected.
 */
export const REVIEWED_METRICS = {
  occurrenceCount: 1341,
  duplicateNames: 36,
  duplicateNameOccurrences: 83,
  maxExactNameReductions: 47,
  verbFamilyAddCreateSetConfigure: 820,
} as const;

/**
 * Source-backed justification for an occurrence's A-F class. The text is
 * deterministic and cites the concrete tool/action and resolved canonical id,
 * never a generic "primary baseline" bucket.
 */
export function justificationFor(
  classification: Classification,
  role: Role,
  tool: string,
  action: string,
): string {
  const roleTag = role === 'alias' ? ' (alias occurrence)' : '';
  switch (classification) {
    case 'A':
      return `True duplicate of "${action}" surfaced under multiple tools; collapses to one shared canonical capability cap:shared:${action}${roleTag}.`;
    case 'C':
      return `Distinct target/semantics: single declaration of "${action}" under "${tool}" (cap:${tool}:${action}); no shared name, no preset/alias/composite/obsolete marker.`;
    case 'E':
      return `Preset/workflow scaffold "${action}" exposed as a single convenience action under "${tool}".`;
    case 'F':
      return `Obsolete / version-specific action "${action}" under "${tool}"; retained for compatibility.`;
    case 'B':
      return 'Alias of another canonical capability.';
    case 'D':
      return 'Composite action composing several lower-level actions.';
  }
}

/** Disposition chosen for a duplicate-group secondary occurrence. */
export const MERGE_SECONDARY_DISPOSITION: Disposition = 'alias';
