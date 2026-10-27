/** Shared helpers for the inspect capability record tests. */
import { ALL_UNFOLDED_CAPABILITY_RECORDS } from '../unfolded.js';

// The shipped catalog folds sibling records into families; per-action facts
// (effects, routing, normalization) are pinned on the authored, unfolded records.
export const INSPECT_UNFOLDED_RECORDS = ALL_UNFOLDED_CAPABILITY_RECORDS.filter((record) => record.routing.parentTool === 'inspect');

export function findByAction(action: string) {
	const record = INSPECT_UNFOLDED_RECORDS.find((r) => r.legacyIds[0].action === action);
	if (!record) throw new Error(`Record not found for action: ${action}`);
	return record;
}
