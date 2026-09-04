import type { CapabilityRecordSource } from '../../index.js';
import { utilityRecord } from '../utility/helpers.js';

const T = 'manage_networking' as const;
const ENHANCED = ['EnhancedInput'] as const;
const i = (action: string, summary: string, params: readonly string[], required: readonly string[], outputs: readonly string[] = [], outputRequired: readonly string[] = [], enhanced = true, effect: 'read' | 'write' | 'destructive' = 'write'): CapabilityRecordSource => utilityRecord({
  tool: T, action, family: 'input', summary, params, required,
  outputs, outputRequired, plugins: enhanced ? ENHANCED : [], effect,
  safeToRetry: effect === 'read', dispatchAction: 'manage_input',
});

export const NETWORKING_INPUT_RECORDS: readonly CapabilityRecordSource[] = [
  i('create_input_action', 'Create an Enhanced Input Action asset (valueType: digital, axis1d, axis2d, or axis3d — movement/look actions need axes).', ['name', 'path', 'valueType'], ['name', 'path'], ['assetPath'], ['assetPath']),
  i('create_input_mapping_context', 'Create an Enhanced Input Mapping Context asset.', ['name', 'path', 'priority'], ['name', 'path'], ['assetPath'], ['assetPath']),
  i('add_mapping', 'Add an Enhanced Input mapping with optional trigger and modifier types.', ['contextPath', 'actionPath', 'key', 'triggerType', 'modifierType'], ['contextPath', 'actionPath', 'key']),
  i('remove_mapping', 'Remove an Enhanced Input mapping.', ['contextPath', 'actionPath', 'key'], ['contextPath', 'actionPath'], [], [], true, 'destructive'),
  i('add_legacy_action_mapping', 'Add a legacy action mapping.', ['name', 'actionName', 'key', 'shift', 'ctrl', 'alt', 'cmd'], ['key'], [], [], false),
  i('remove_legacy_action_mapping', 'Remove a legacy action mapping.', ['name', 'actionName', 'key', 'shift', 'ctrl', 'alt', 'cmd'], ['key'], [], [], false, 'destructive'),
  i('add_legacy_axis_mapping', 'Add a legacy axis mapping.', ['name', 'axisName', 'key', 'scale'], ['key'], [], [], false),
  i('remove_legacy_axis_mapping', 'Remove a legacy axis mapping.', ['name', 'axisName', 'key', 'scale'], ['key'], [], [], false, 'destructive'),
  i('map_input_action', 'Map an Enhanced Input Action in a context (key only — no trigger/modifier support; use add_mapping for those).', ['contextPath', 'actionPath', 'key'], ['contextPath', 'actionPath', 'key']),
  i('set_input_trigger', 'Set an Enhanced Input trigger.', ['actionPath', 'triggerType'], ['actionPath', 'triggerType']),
  i('set_input_modifier', 'Set an Enhanced Input modifier.', ['contextPath', 'actionPath', 'key', 'modifierType'], ['actionPath', 'modifierType']),
  i('enable_input_mapping', 'Enable an Enhanced Input Mapping Context.', ['contextPath', 'priority'], ['contextPath']),
  i('disable_input_action', 'Disable an Enhanced Input Action.', ['actionPath'], ['actionPath']),
  i('get_input_info', 'Read input asset and mapping state.', ['assetPath'], ['assetPath'],
    ['assetPath', 'assetClass', 'assetName', 'existsAfter', 'type', 'valueType', 'consumeInput', 'mappingCount'],
    ['assetPath', 'assetClass', 'assetName', 'existsAfter'], true, 'read'),
];
