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
    parentTool: 'control_editor', action: 'invoke_reflected_function', dispatchAction: 'control_editor', dispatchMode: 'tool',
    domain: D, family: F,
    summary: 'Call one reflected UFunction on a plugin\'s live object, marshalling arguments through the function\'s own property chain. No signature is hardcoded: whatever describe_reflected_api reports for the installed build is what this accepts, so an integration stays correct across plugin updates instead of silently passing a stale parameter list. Return and out parameters come back in `outputs`. Refuses when only the class default object exists, because invoking on the CDO mutates shared defaults and never reaches the running instance. This is arbitrary in-process invocation — it can reach any reflected function on any resolvable object — so it demands elevated consent.',
    whenToUse: [
      'A plugin exposes the needed operation only as a UFUNCTION and no native capability covers it.',
      'describe_reflected_api confirmed the function and its current parameters.',
    ],
    whenNotToUse: [
      'A native capability already covers the operation — prefer it; a reflected call carries none of the safety wrappers, undo transactions, or path validation the domain handlers apply.',
      'The signature has not been confirmed against the installed build.',
    ],
    inputProps: {
      className: { type: 'string', description: 'Reflected class name, for example "FabBrowserApi".' },
      functionName: { type: 'string', description: 'Function name exactly as reported by describe_reflected_api.' },
      arguments: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Argument values keyed by parameter name. Converted per-property, so structs and arrays are accepted in their JSON form. Omitted parameters keep their zero-initialised default and are listed in unsetParameters.' },
    },
    outputProps: {
      className: { type: 'string', description: 'Class acted on.' },
      functionName: { type: 'string', description: 'Function invoked.' },
      resolvedObject: { type: 'string', description: 'Path name of the object the call was made on.' },
      outputs: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Return value and out parameters, keyed by parameter name.' },
      unsetParameters: { type: 'array', items: { type: 'string' }, description: 'Parameters left at their default because no argument was supplied.' },
    },
    required: ['className', 'functionName'],
    effect: 'destructive',
    policyOverride: { consent: 'elevated' },
    costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'invoke_reflected_function', className: 'FabBrowserApi', functionName: 'GetAuthToken' },
    exampleOutput: { success: true, functionName: 'GetAuthToken' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
    normalizationRationale: 'Distinct reflected-invocation capability, paired with describe_reflected_api. Marshals through the UFunction property chain rather than a recorded signature. Classed destructive because it can invoke any reflected function, including ones that mutate project state irreversibly.',
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'describe_reflected_api', dispatchAction: 'control_editor', dispatchMode: 'tool',
    domain: D, family: F,
    summary: 'Enumerate the reflected UFunction surface of another plugin\'s live bridge object, by class name. Plugins that host a web view register a UObject with the page (Fab binds FabBrowserApi as window.ue.fab), and because that object is reflected it is reachable by name without linking the plugin. The answer is read from the INSTALLED build at call time, so an integration never has to hardcode another plugin\'s contract or freeze a copy of it that goes stale on the next engine update.',
    whenToUse: [
      'An integration must discover what a plugin currently exposes, rather than assume a signature recorded earlier.',
      'A plugin API appears to have changed and the live surface needs checking.',
    ],
    whenNotToUse: [
      'The class is a plain C++ type with no UCLASS reflection — nothing is discoverable.',
      'A native capability already covers the operation.',
    ],
    inputProps: {
      className: { type: 'string', description: 'Reflected class name without prefix, for example "FabBrowserApi". The live instance is preferred; the class default object is the fallback when no instance exists yet.' },
      filter: { type: 'string', description: 'Case-sensitive substring matched against function names.' },
    },
    outputProps: {
      className: { type: 'string', description: 'Class that was resolved.' },
      resolvedObject: { type: 'string', description: 'Path name of the object the surface was read from.' },
      isDefaultObject: { type: 'boolean', description: 'True when only the CDO existed, which usually means the owning window has never been opened.' },
      functions: {
        type: 'array',
        description: 'Reflected functions, name-sorted.',
        items: {
          type: 'object',
          additionalProperties: false,
          properties: {
            name: { type: 'string', description: 'Function name as reflected.' },
            parameterCount: { type: 'number', description: 'Declared parameters, including the return value.' },
            parameters: {
              type: 'array',
              description: 'Parameters in declaration order.',
              items: {
                type: 'object',
                additionalProperties: false,
                properties: {
                  name: { type: 'string', description: 'Parameter name.' },
                  cppType: { type: 'string', description: 'Reflected C++ type, for example FString.' },
                  isReturn: { type: 'boolean', description: 'True for the return value.' },
                  isOut: { type: 'boolean', description: 'True for an out parameter that is not the return value.' },
                },
              },
            },
          },
        },
      },
      functionCount: { type: 'number', description: 'Functions returned.' },
    },
    required: ['className'],
    effect: 'read',
    costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'describe_reflected_api', className: 'FabBrowserApi' },
    exampleOutput: { success: true, functionCount: 14 },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
    normalizationRationale: 'Distinct read-only reflection-introspection capability. Resolves the class through the UObject graph rather than linking the owning plugin, so it reports the surface of whatever version is installed.',
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'open_editor_tab', dispatchAction: 'control_editor', dispatchMode: 'tool',
    domain: D, family: F,
    summary: 'Open a registered editor tab by id via FGlobalTabmanager, the same path the Window menu uses. Content-source plugins register their windows globally — Bridge as "BridgeTab", Fab as "FabTab" — so this reaches them without depending on either plugin. This is also the correct way to authenticate against those services: each owns its own sign-in and persists its own session, so opening its window lets it log in on its own terms rather than reimplementing a login.',
    whenToUse: [
      'A Quixel/Fab capability reported NOT_AUTHENTICATED and the owning window must be opened so the user can sign in.',
      'An editor panel registered by a plugin needs to be brought up.',
    ],
    whenNotToUse: ['An asset editor should be opened for a specific asset (use open_asset).'],
    inputProps: {
      tabId: { type: 'string', description: 'Registered nomad tab id, for example "BridgeTab" (Quixel Bridge) or "FabTab" (Fab).' },
    },
    outputProps: {
      tabId: { type: 'string', description: 'Tab id acted on.' },
      opened: { type: 'boolean', description: 'True when the tab manager returned a live tab.' },
    },
    required: ['tabId'],
    effect: 'write',
    costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'open_editor_tab', tabId: 'BridgeTab' },
    exampleOutput: { success: true, opened: true },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
    normalizationRationale: 'Distinct editor-tab invocation capability. Resolves the tab through FGlobalTabmanager rather than linking the owning plugin, so it works for any registered nomad tab.',
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'console_command', dispatchAction: 'console_command', dispatchMode: 'action',
    domain: D, family: F,
    topics: ['console command', 'exec command', 'run command', 'stat fps', 'cheat command'],
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
