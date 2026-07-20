/**
 * Command and preferences records: console_command, execute_command,
 * set_preferences.
 *
 * Grounded in src/tools/handlers/editor/editor-asset-actions.ts.
 * console_command and execute_command both cross-parent dispatch to the
 * console_command bridge action; the true duplicate is shared with
 * system_control (cap:shared:console_command / cap:shared:execute_command).
 * set_preferences routes through control_editor but is documented as a
 * potential project-setting surface that could be confused with
 * system_control set_project_setting.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildCoreRecord } from '../core/builder.js';
import { P } from './properties.js';

const F = 'command';
const D = 'editor';

export const COMMAND_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'control_editor', action: 'console_command', dispatchAction: 'console_command', dispatchMode: 'action',
    domain: D, family: F,
    summary: 'Execute an Unreal console command via cross-parent dispatch to the console_command bridge action.',
    whenToUse: ['A console command must be run from the editor context.'],
    whenNotToUse: ['A dedicated action exists for the operation.'],
    inputProps: { command: P.command },
    required: ['command'],
    effect: 'write',
    costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'console_command', command: 'r.SetRes 1920x1080' },
    exampleOutput: { success: true, message: 'Command executed' },
    normalizationClass: 'A_TRUE_DUPLICATE',
    normalizationRationale: 'Cross-parent dispatch to the console_command bridge action; true duplicate shared across control_editor and system_control (cap:shared:console_command).',
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'execute_command', dispatchAction: 'console_command', dispatchMode: 'action',
    domain: D, family: F,
    summary: 'Execute a console command (alias cross-parent to the console_command bridge action).',
    whenToUse: ['A command must be executed with explicit validation.'],
    whenNotToUse: ['The console_command action is sufficient.'],
    inputProps: { command: P.command },
    required: ['command'],
    effect: 'write',
    costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'execute_command', command: 'stat fps' },
    exampleOutput: { success: true, message: 'Command executed', action: 'execute_command' },
    outputProps: { action: P.action },
    normalizationClass: 'A_TRUE_DUPLICATE',
    normalizationRationale: 'Cross-parent dispatch to the console_command bridge action; true duplicate shared across control_editor and system_control (cap:shared:execute_command). TS re-badges response action as execute_command.',
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'set_preferences', domain: D, family: F,
    summary: 'Set editor preferences for a category. Distinct from system_control set_project_setting.',
    whenToUse: ['Editor preferences must be configured for a category.'],
    whenNotToUse: ['Project settings are needed (use system_control set_project_setting).'],
    inputProps: { category: P.category, preferences: P.preferences },
    required: ['category', 'preferences'],
    effect: 'write', behavior: { idempotency: 'idempotent' },
    costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'set_preferences', category: 'Editor', preferences: { bUseSmallToolBarIcons: true } },
    exampleOutput: { success: true, message: 'Preferences set' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
    normalizationRationale: 'Editor preferences (display/tool config); potential misroute to system_control set_project_setting for project-level settings. Distinct control_editor verb.',
  }),
];
