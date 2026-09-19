/**
 * manage_ai capability record catalog: 65 authored CapabilityRecordSource
 * entries (add 16, configure 17, create/read 17, set 15), folded by
 * MANAGE_AI_FOLDS into MANAGE_AI_SOURCES. The authored count is pinned by
 * manage-ai-records.test.ts.
 *
 * Composed from four sharded action-property maps, concatenated in the
 * canonical manage_ai action sequence (the generator emits the parent action
 * enum from this order verbatim).
 *
 * These records no longer use ../compact-parent.ts. That builder gave every AI
 * action the same 7-property stub (action/assetPath/blueprintPath/actorName/
 * name/path/properties), which collapsed the manage_ai parent surface to 8
 * properties and dropped 86 real parameters. Each record below instead declares
 * the EXACT properties its own handler reads, grounded in:
 *   - the legacy input schema recovered from HEAD:src/tools/definitions/
 *     gameplay/ai/manage-ai-{behavior,navigation,runtime}-properties.ts;
 *   - the TS handlers (handlers/ai/ai-handlers.ts, ai-utility-actions.ts,
 *     handlers/navigation/navigation-handlers.ts, and the behavior-tree graph
 *     route via orchestration/consolidated-handler-registration.ts:205-206);
 *   - the native AI / Navigation / BehaviorTree domains under
 *     plugins/McpAutomationBridge/.../Private/Domains/.
 *
 * The parent union is therefore derived per action by the generator
 * (scripts/canonical-registry/parent-derivation.ts) rather than applied
 * wholesale: no action advertises a parameter its handler never reads.
 *
 * The 3 formerly hidden AI routes (create_nav_modifier, set_ai_movement,
 * set_ai_perception) have since been PROMOTED into this set, each marked
 * `post-migration` so the pre-gateway normalization audit still excludes them.
 * ../hidden-routes.ts keeps their route dispositions as the evidence trail.
 */
import type { CapabilityRecordSource } from '../../../index.js';

import { AI_ADD_RECORDS } from './add-actions.data.js';
import { AI_CONFIGURE_RECORDS } from './configure-actions.data.js';
import { AI_CREATE_READ_RECORDS } from './create-read-actions.data.js';
import { AI_SET_RECORDS } from './set-actions.data.js';
import { applyFolds } from '../../shared/fold.js';
import { MANAGE_AI_FOLDS } from '../../folds/manage-ai.folds.js';

/** The authored records before folding; per-action contract tests pin these. */
export const MANAGE_AI_UNFOLDED_SOURCES: readonly CapabilityRecordSource[] = [
  ...AI_ADD_RECORDS,
  ...AI_CONFIGURE_RECORDS,
  ...AI_CREATE_READ_RECORDS,
  ...AI_SET_RECORDS,
];

export const MANAGE_AI_SOURCES: readonly CapabilityRecordSource[] = applyFolds(MANAGE_AI_UNFOLDED_SOURCES, MANAGE_AI_FOLDS, 'manage_ai');
