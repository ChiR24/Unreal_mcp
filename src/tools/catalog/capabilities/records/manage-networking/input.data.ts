import type { CapabilityRecordSource } from '../../model.js';
import { utilityRecord, withTopics } from '../utility/utility-record-builders.js';

const T = 'manage_networking' as const;
const ENHANCED = ['EnhancedInput'] as const;
const i = (action: string, summary: string, params: readonly string[], required: readonly string[], outputs: readonly string[] = [], outputRequired: readonly string[] = [], enhanced = true, effect: 'read' | 'write' | 'destructive' = 'write'): CapabilityRecordSource => utilityRecord({
  tool: T, action, family: 'input', summary, params, required,
  outputs, outputRequired, plugins: enhanced ? ENHANCED : [], effect,
  safeToRetry: effect === 'read', dispatchAction: 'manage_input',
});

export const NETWORKING_INPUT_RECORDS: readonly CapabilityRecordSource[] = [
  withTopics(i('create_input_action', 'Create an Enhanced Input Action asset (valueType: digital, axis1d, axis2d, or axis3d — movement/look actions need axes).', ['name', 'path', 'valueType'], ['name', 'path'], ['assetPath'], ['assetPath']), ['input action', 'enhanced input', 'new input action', 'key binding']),
  // A mapping context asset has no priority of its own; the priority is given when it is enabled.
  withTopics(i('create_input_mapping_context', 'Create an Enhanced Input Mapping Context asset; its priority is chosen when it is enabled (enable_input_mapping).', ['name', 'path'], ['name', 'path'], ['assetPath'], ['assetPath']), ['input mapping', 'mapping context', 'imc', 'key mapping', 'enhanced input mapping']),
  i('add_mapping', 'Add an Enhanced Input mapping with optional trigger and modifier types.', ['contextPath', 'actionPath', 'key', 'triggerType', 'modifierType'], ['contextPath', 'actionPath', 'key']),
  i('remove_mapping', 'Remove an Enhanced Input mapping.', ['contextPath', 'actionPath', 'key'], ['contextPath', 'actionPath'], [], [], true, 'destructive'),
  i('add_legacy_action_mapping', 'Add a legacy action mapping.', ['name', 'actionName', 'key', 'shift', 'ctrl', 'alt', 'cmd'], ['key'], [], [], false),
  i('remove_legacy_action_mapping', 'Remove a legacy action mapping.', ['name', 'actionName', 'key', 'shift', 'ctrl', 'alt', 'cmd'], ['key'], [], [], false, 'destructive'),
  i('add_legacy_axis_mapping', 'Add a legacy axis mapping.', ['name', 'axisName', 'key', 'scale'], ['key'], [], [], false),
  i('remove_legacy_axis_mapping', 'Remove a legacy axis mapping.', ['name', 'axisName', 'key', 'scale'], ['key'], [], [], false, 'destructive'),
  // Same native handler as add_mapping: mapping an action+key again redefines that mapping's trigger and modifier.
  i('map_input_action', 'Map an Enhanced Input Action to a key in a context, with an optional trigger and modifier on that mapping; mapping the same action and key again redefines it.', ['contextPath', 'actionPath', 'key', 'triggerType', 'modifierType'], ['contextPath', 'actionPath', 'key']),
  i('set_input_trigger', 'Add a trigger to an Enhanced Input Action; a trigger of that class already on the action is kept, not stacked.', ['actionPath', 'triggerType'], ['actionPath', 'triggerType']),
  i('set_input_modifier', 'Add a modifier to an Enhanced Input Action, or to one mapping when contextPath and key are given; a modifier of that class already there is kept, not stacked.', ['contextPath', 'actionPath', 'key', 'modifierType'], ['actionPath', 'modifierType']),
  i('enable_input_mapping', 'Enable an Enhanced Input Mapping Context on the PIE local player at a priority; outside PIE it fails with PIE_NOT_RUNNING.', ['contextPath', 'priority'], ['contextPath']),
  i('get_input_info', 'Read input asset and mapping state.', ['assetPath'], ['assetPath'],
    ['assetPath', 'assetClass', 'assetName', 'existsAfter', 'type', 'valueType', 'consumeInput', 'mappingCount'],
    ['assetPath', 'assetClass', 'assetName', 'existsAfter'], true, 'read'),
];
