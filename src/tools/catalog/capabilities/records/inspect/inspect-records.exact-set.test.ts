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
import { EXPECTED_ACTIONS } from './inspect-records.shared.js';

describe('inspect exact-set: 36 records mapped 1:1 to tool actions', () => {
	it('produces exactly 36 capability records', () => {
		expect(INSPECT_RECORD_COUNT).toBe(36);
		expect(INSPECT_SOURCES).toHaveLength(36);
		expect(INSPECT_RECORDS).toHaveLength(36);
	});

	it('maps every inspect tool action to exactly one record legacy ID', () => {
		const legacyKeys = new Set(
			INSPECT_RECORDS.map(
				(r) => `${r.legacyIds[0].tool}::${r.legacyIds[0].action}`,
			),
		);
		for (const action of EXPECTED_ACTIONS) {
			expect(legacyKeys.has(`inspect::${action}`)).toBe(true);
		}
		expect(legacyKeys.size).toBe(36);
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
		const enumSet = new Set(actionProp.enum);
		for (const action of EXPECTED_ACTIONS) {
			expect(enumSet.has(action)).toBe(true);
		}
		expect(enumSet.size).toBe(EXPECTED_ACTIONS.length);
	});

	it('has no duplicate canonical IDs, aliases, or legacy IDs across all 36 records', () => {
		const catalog = parseCapabilityCatalog([...INSPECT_RECORDS]);
		expect(catalog).toHaveLength(36);
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
