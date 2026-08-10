/**
 * Focused tests: system_control exact-set — 52 records mapped 1:1 to the
 * system_control tool action enum (33 explicit + 19 PERFORMANCE_ACTIONS),
 * unique IDs, and canonical enum-order emission.
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
import { ALL_52_ACTIONS } from './system-control-test-helpers.js';

describe('system_control exact-set: 52 records mapped 1:1 to tool actions', () => {
	it('produces exactly 52 capability records', () => {
		expect(SYSTEM_CONTROL_RECORD_COUNT).toBe(52);
		expect(SYSTEM_CONTROL_SOURCES).toHaveLength(52);
		expect(SYSTEM_CONTROL_RECORDS).toHaveLength(52);
	});

	it('maps every system_control tool action to exactly one record legacy ID', () => {
		const legacyKeys = new Set(
			SYSTEM_CONTROL_RECORDS.flatMap((r) =>
				r.legacyIds.map((li) => `${li.tool}::${li.action}`),
			),
		);
		for (const action of ALL_52_ACTIONS) {
			expect(legacyKeys.has(`system_control::${action}`)).toBe(true);
		}
		expect(legacyKeys.size).toBe(52);
	});

	it('the tool definition action enum matches the union of action sets exactly (52)', () => {
		const props = systemControlToolDefinition.inputSchema.properties as Record<
			string,
			{ enum?: readonly string[] }
		>;
		const actionProp = props.action;
		if (!actionProp?.enum) {
			throw new TypeError('system_control action enum is unavailable');
		}
		const enumSet = new Set(actionProp.enum);
		for (const action of ALL_52_ACTIONS) {
			expect(enumSet.has(action)).toBe(true);
		}
		expect(enumSet.size).toBe(ALL_52_ACTIONS.length);
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

	it('has no duplicate canonical IDs, aliases, or legacy IDs across all 52 records', () => {
		const catalog = parseCapabilityCatalog([...SYSTEM_CONTROL_RECORDS]);
		expect(catalog).toHaveLength(52);
	});
});
