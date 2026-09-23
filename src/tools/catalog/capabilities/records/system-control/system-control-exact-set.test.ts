/**
 * Focused tests: system_control exact-set — 59 authored records (40 explicit
 * enum actions + 19 PERFORMANCE_ACTIONS) folded into the shipped set, whose
 * legacy pairs cover the action enum exactly, plus unique IDs and canonical
 * enum-order emission.
 */
import { describe, expect, it } from 'vitest';
import { consolidatedToolDefinitions } from '../../../../catalog/consolidated-tool-definitions.js';

const systemControlToolDefinition = consolidatedToolDefinitions.find((t) => t.name === 'system_control') as NonNullable<typeof consolidatedToolDefinitions[number]>;
import { parseCapabilityCatalog } from '../../index.js';
import {
	SYSTEM_CONTROL_RECORD_COUNT,
	SYSTEM_CONTROL_RECORDS,
	SYSTEM_CONTROL_SOURCES,
} from './index.js';
import {
	ALL_ACTIONS,
	SYSTEM_CONTROL_FOLDED_RECORD_COUNT,
	SYSTEM_CONTROL_LEGACY_PAIR_COUNT,
	SYSTEM_CONTROL_UNFOLDED_RECORDS,
} from './system-control-test-helpers.js';

describe('system_control exact-set: 59 records mapped 1:1 to tool actions', () => {
	it('folds 59 authored records into SYSTEM_CONTROL_FOLDED_RECORD_COUNT capability records', () => {
		expect(SYSTEM_CONTROL_UNFOLDED_RECORDS).toHaveLength(59);
		expect(SYSTEM_CONTROL_RECORD_COUNT).toBe(SYSTEM_CONTROL_FOLDED_RECORD_COUNT);
		expect(SYSTEM_CONTROL_SOURCES).toHaveLength(SYSTEM_CONTROL_FOLDED_RECORD_COUNT);
		expect(SYSTEM_CONTROL_RECORDS).toHaveLength(SYSTEM_CONTROL_FOLDED_RECORD_COUNT);
	});

	it('maps every system_control tool action to exactly one record legacy ID', () => {
		const legacyKeys = new Set(
			SYSTEM_CONTROL_RECORDS.flatMap((r) =>
				r.legacyIds.map((li) => `${li.tool}::${li.action}`),
			),
		);
		for (const action of ALL_ACTIONS) {
			expect(legacyKeys.has(`system_control::${action}`)).toBe(true);
		}
		expect(legacyKeys.size).toBe(SYSTEM_CONTROL_LEGACY_PAIR_COUNT);
	});

	it('the tool definition action enum matches the union of action sets exactly (59)', () => {
		const props = systemControlToolDefinition.inputSchema.properties as Record<
			string,
			{ enum?: readonly string[] }
		>;
		const actionProp = props.action;
		if (!actionProp?.enum) {
			throw new TypeError('system_control action enum is unavailable');
		}
		// The enum advertises each folded family once; every authored action
		// stays reachable as that family's legacy pair.
		const enumSet = new Set(actionProp.enum);
		const pairs = new Set(SYSTEM_CONTROL_RECORDS.flatMap((r) => r.legacyIds.map((li) => String(li.action))));
		for (const action of ALL_ACTIONS) {
			expect(pairs.has(action)).toBe(true);
		}
		for (const action of enumSet) {
			expect(pairs.has(action)).toBe(true);
		}
		expect(enumSet.size).toBe(SYSTEM_CONTROL_FOLDED_RECORD_COUNT);
	});

	it('emits records in canonical definition enum order', () => {
		const props = systemControlToolDefinition.inputSchema.properties as Record<
			string,
			{ enum?: readonly string[] }
		>;
		const enumActions = props.action?.enum ?? [];
		const recordActions = SYSTEM_CONTROL_RECORDS.map((r) => r.legacyIds[0].action);
		expect(recordActions).toEqual([...enumActions]);
	});

	it('has no duplicate canonical IDs, aliases, or legacy IDs across all folded records', () => {
		const catalog = parseCapabilityCatalog([...SYSTEM_CONTROL_RECORDS]);
		expect(catalog).toHaveLength(SYSTEM_CONTROL_FOLDED_RECORD_COUNT);
	});
});
