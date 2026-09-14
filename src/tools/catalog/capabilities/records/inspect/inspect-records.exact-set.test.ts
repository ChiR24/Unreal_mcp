/**
 * Exact-set tests for the inspect capability record catalog.
 *
 * Proves: exactly 36 records mapped 1:1 to tool actions, unique canonical
 * IDs/aliases/legacy IDs, and canonical enum-order emission. Does not touch
 * the shared core builder, aggregate, pilots, or native code.
 */
import { describe, expect, it } from 'vitest';
import { consolidatedToolDefinitions } from '../../../../catalog/consolidated-tool-definitions.js';

const inspectToolDefinition = consolidatedToolDefinitions.find((t) => t.name === 'inspect') as NonNullable<typeof consolidatedToolDefinitions[number]>;
import { parseCapabilityCatalog } from '../../index.js';
import { INSPECT_RECORD_COUNT, INSPECT_RECORDS, INSPECT_SOURCES } from './index.js';
import {
	EXPECTED_ACTIONS,
	INSPECT_FOLDED_RECORD_COUNT,
	INSPECT_LEGACY_PAIR_COUNT,
	INSPECT_UNFOLDED_RECORDS,
} from './inspect-records.shared.js';

describe('inspect exact-set: 36 records mapped 1:1 to tool actions', () => {
	it('folds 36 authored records into 16 capability records', () => {
		expect(INSPECT_UNFOLDED_RECORDS).toHaveLength(36);
		expect(INSPECT_RECORD_COUNT).toBe(INSPECT_FOLDED_RECORD_COUNT);
		expect(INSPECT_SOURCES).toHaveLength(INSPECT_FOLDED_RECORD_COUNT);
		expect(INSPECT_RECORDS).toHaveLength(INSPECT_FOLDED_RECORD_COUNT);
	});

	it('maps every inspect tool action to exactly one record legacy ID', () => {
		const legacyKeys = new Set(
			INSPECT_RECORDS.flatMap((r) => r.legacyIds.map((li) => `${li.tool}::${li.action}`)),
		);
		for (const action of EXPECTED_ACTIONS) {
			expect(legacyKeys.has(`inspect::${action}`)).toBe(true);
		}
		expect(legacyKeys.size).toBe(INSPECT_LEGACY_PAIR_COUNT);
	});

	it('the tool definition action enum matches the 36-action set exactly', () => {
		const props = inspectToolDefinition.inputSchema.properties as Record<
			string,
			{ enum?: readonly string[] }
		>;
		const actionProp = props.action;
		if (!actionProp?.enum) {
			throw new TypeError('inspect action enum is unavailable');
		}
		// The enum advertises each folded family once; every authored action
		// stays reachable as that family's legacy pair.
		const enumSet = new Set(actionProp.enum);
		const pairs = new Set(INSPECT_RECORDS.flatMap((r) => r.legacyIds.map((li) => li.action)));
		for (const action of EXPECTED_ACTIONS) {
			expect(pairs.has(action)).toBe(true);
		}
		for (const action of enumSet) {
			expect(pairs.has(action)).toBe(true);
		}
		expect(enumSet.size).toBe(INSPECT_FOLDED_RECORD_COUNT);
	});

	it('has no duplicate canonical IDs, aliases, or legacy IDs across all folded records', () => {
		const catalog = parseCapabilityCatalog([...INSPECT_RECORDS]);
		expect(catalog).toHaveLength(INSPECT_FOLDED_RECORD_COUNT);
	});

	it('emits records in canonical tool-definition enum order', () => {
		const props = inspectToolDefinition.inputSchema.properties as Record<
			string,
			{ enum?: readonly string[] }
		>;
		const actionProp = props.action;
		if (!actionProp?.enum) {
			throw new TypeError('inspect action enum is unavailable');
		}
		const recordOrder = INSPECT_RECORDS.map((r) => r.legacyIds[0].action);
		expect(recordOrder).toEqual([...actionProp.enum]);
	});
});
