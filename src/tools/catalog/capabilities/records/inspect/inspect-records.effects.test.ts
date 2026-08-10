/**
 * Effect-distribution tests for the inspect capability record catalog.
 *
 * Proves: delete_object is the only destructive action and is not safe to
 * retry, the exact write-action set, and that the remaining 30 actions are
 * read-only. Does not touch the shared core builder, aggregate, pilots, or
 * native code.
 */
import { describe, expect, it } from 'vitest';
import { INSPECT_RECORDS } from './index.js';
import { findByAction } from './inspect-records.shared.js';

describe('inspect effects: read/write/destructive distribution', () => {
	it('delete_object is the only destructive action and is not safe to retry', () => {
		const destructive = INSPECT_RECORDS.filter(
			(r) => r.behavior.effect === 'destructive',
		);
		expect(destructive.map((r) => r.legacyIds[0].action)).toEqual([
			'delete_object',
		]);
		const del = findByAction('delete_object');
		expect(del.behavior.safeToRetry).toBe(false);
		expect(del.policy.consent).toBe('explicit');
	});

	it('write actions are exactly set_property, set_component_property, add_tag, create_snapshot, restore_snapshot', () => {
		const writeActions = INSPECT_RECORDS.filter(
			(r) => r.behavior.effect === 'write',
		)
			.map((r) => r.legacyIds[0].action)
			.sort();
		expect(writeActions).toEqual([
			'add_tag',
			'create_snapshot',
			'restore_snapshot',
			'set_component_property',
			'set_property',
		]);
	});

	it('the remaining 30 actions are read-only', () => {
		const readCount = INSPECT_RECORDS.filter(
			(r) => r.behavior.effect === 'read',
		).length;
		expect(readCount).toBe(30);
	});
});
