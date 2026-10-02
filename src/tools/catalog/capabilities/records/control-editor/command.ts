/**
 * Command and preferences records: console_command, execute_command,
 * set_preferences.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildCoreRecord } from '../core/builder.js';
import { P } from './properties.js';

const F = 'command';
const D = 'editor';

export const COMMAND_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'control_editor', action: 'invoke_reflected_function', dispatchAction: 'control_editor',
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
    costLatency: 'interactive',
    exampleInput: { action: 'invoke_reflected_function', className: 'FabBrowserApi', functionName: 'GetAuthToken' },
    exampleOutput: { success: true, functionName: 'GetAuthToken' },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'describe_reflected_api', dispatchAction: 'control_editor',
    domain: D, family: F,
    topics: ['class functions', 'function parameters', 'plugin surface'],
    summary: 'List a class\'s reflected functions (a plugin\'s live bridge object such as FabBrowserApi, or any engine class): each with its parameters, C++ types and return or out flags, read from the installed build; filter narrows by function name.',
    whenToUse: [
      'An integration must discover what a plugin currently exposes, rather than assume a signature recorded earlier.',
      'A plugin API appears to have changed and the live surface needs checking.',
    ],
    whenNotToUse: [
      'The class is a plain C++ type with no UCLASS reflection — nothing is discoverable.',
      'A native capability already covers the operation.',
    ],
    inputProps: {
      className: { type: 'string', description: 'Reflected class, either bare ("FabBrowserApi", "DirectionalLightComponent") or as a full path ("/Script/Engine.DirectionalLightComponent"). The live instance is preferred; the class default object is the fallback when no instance exists yet.' },
      // Every other class-taking capability publishes `classPath`; refusing it
      // here cost a round trip for no reason.
      classPath: { type: 'string', description: 'Alias for className, the spelling the rest of the catalog uses.' },
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
    required: [],
    requiredOneOf: ['className', 'classPath'],
    effect: 'read',
    costLatency: 'interactive',
    exampleInput: { action: 'describe_reflected_api', className: 'FabBrowserApi' },
    exampleOutput: { success: true, functionCount: 14 },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'list_editor_toolsets', dispatchAction: 'control_editor',
    domain: D, family: F,
    topics: ['epic toolsets', 'toolset registry', 'epic mcp tools', 'state tree', 'control rig', 'pcg', 'gameplay abilities', 'niagara toolset', 'mvvm'],
    summary: 'List the tools of Epic\'s editor toolsets (the Toolset Registry that ships with Unreal Engine 5.8: StateTree, PCG, GAS, Niagara, UMG, MVVM, physics and more, as their plugins are enabled), each with a description and an effect (read, write, destructive) judged from its name; query narrows them, and toolName returns one tool\'s input schema for call_editor_tool.',
    whenToUse: [
      'No native capability covers the work and Epic ships a toolset for it on Unreal Engine 5.8.',
      'The input schema of an Epic toolset tool is needed before call_editor_tool.',
    ],
    whenNotToUse: [
      'A native capability already covers the operation: prefer it, it carries this plugin\'s path checks, undo steps and receipts.',
      'The editor runs an engine before 5.8 (the registry does not exist there; the call answers EDITOR_TOOLSETS_UNAVAILABLE).',
    ],
    inputProps: {
      query: { type: 'string', description: 'Words that must all appear in a tool\'s name or description (any case), e.g. "state tree add".' },
      toolset: { type: 'string', description: 'List only the tools of this toolset, by the name the toolsets list shows.' },
      toolName: { type: 'string', description: 'Full name of one tool, Toolset.Tool: returns its schema (description, inputSchema) and effect instead of the list.' },
      limit: { type: 'integer', minimum: 1, maximum: 200, description: 'Most tools listed (default 50); matchedTools and hasMore tell what was left out.' },
    },
    outputProps: {
      toolsets: { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, 'x-unreal-reflection-boundary': true, description: 'Every registered toolset: name, description, toolCount.' },
      tools: { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, 'x-unreal-reflection-boundary': true, description: 'Matching tools: name (Toolset.Tool), description, effect (read, write or destructive; blocked for a tool that would run scripts or console code, which is never called).' },
      matchedTools: { type: 'integer', description: 'Tools that matched, before limit.' },
      hasMore: { type: 'boolean', description: 'True when more tools matched than were listed.' },
      tool: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'With toolName: that tool\'s schema as Epic publishes it (name, description, inputSchema) plus effect.' },
    },
    required: [],
    effect: 'read',
    plugins: ['ToolsetRegistry'],
    costLatency: 'interactive',
    exampleInput: { action: 'list_editor_toolsets', query: 'selected actors' },
    exampleOutput: { success: true, matchedTools: 1 },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'call_editor_tool', dispatchAction: 'control_editor',
    domain: D, family: F,
    topics: ['run epic tool', 'epic toolset call', 'toolset registry'],
    summary: 'Run one tool of Epic\'s editor toolsets (Unreal Engine 5.8 Toolset Registry) with a JSON input matching the inputSchema list_editor_toolsets returned, and answer with its output once it finishes. A tool whose name starts with a destructive verb (Delete, Remove, Destroy, Clear, Reset ...) is refused with DESTRUCTIVE_EDITOR_TOOL: run it with call_editor_tool_destructive. A tool that would run scripts or console code is never called (EDITOR_TOOL_BLOCKED).',
    whenToUse: ['list_editor_toolsets found an Epic tool for work no native capability covers.'],
    whenNotToUse: [
      'A native capability covers the operation: prefer it.',
      'The tool deletes or resets something: use call_editor_tool_destructive.',
    ],
    inputProps: {
      toolName: { type: 'string', description: 'Full tool name exactly as list_editor_toolsets lists it: the toolset, then the tool, e.g. EditorToolset.EditorAppToolset.GetSelectedActors.' },
      input: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'The tool\'s arguments, shaped by the inputSchema list_editor_toolsets returned for toolName. Omit for a tool without arguments.' },
    },
    outputProps: {
      toolName: { type: 'string', description: 'Tool that ran.' },
      effect: { type: 'string', description: 'read or write, as judged from the tool\'s name.' },
      output: { 'x-unreal-reflection-boundary': true, description: 'The tool\'s own result: its JSON output parsed, or the raw text when it is not JSON.' },
    },
    required: ['toolName'],
    effect: 'write',
    plugins: ['ToolsetRegistry'],
    costLatency: 'interactive',
    exampleInput: { action: 'call_editor_tool', toolName: 'EditorToolset.EditorAppToolset.GetSelectedActors' },
    exampleOutput: { success: true, toolName: 'EditorToolset.EditorAppToolset.GetSelectedActors' },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'call_editor_tool_destructive', dispatchAction: 'control_editor',
    domain: D, family: F,
    topics: ['delete with epic tool', 'remove with epic toolset'],
    summary: 'Run one tool of Epic\'s editor toolsets that deletes or resets editor state (its name starts with Delete, Remove, Destroy, Clear, Reset or a similar verb), under the destructive scope and elevated consent; any other tool runs here too. A tool that would run scripts or console code is never called.',
    whenToUse: ['call_editor_tool refused a tool with DESTRUCTIVE_EDITOR_TOOL and the deletion is intended.'],
    whenNotToUse: ['The tool only reads or adds: use call_editor_tool, which needs no consent.'],
    inputProps: {
      toolName: { type: 'string', description: 'Full tool name exactly as list_editor_toolsets lists it: the toolset, then the tool, e.g. EditorToolset.EditorAppToolset.GetSelectedActors.' },
      input: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'The tool\'s arguments, shaped by the inputSchema list_editor_toolsets returned for toolName.' },
    },
    outputProps: {
      toolName: { type: 'string', description: 'Tool that ran.' },
      effect: { type: 'string', description: 'destructive, read or write, as judged from the tool\'s name.' },
      output: { 'x-unreal-reflection-boundary': true, description: 'The tool\'s own result: its JSON output parsed, or the raw text when it is not JSON.' },
    },
    required: ['toolName'],
    effect: 'destructive',
    policyOverride: { consent: 'elevated' },
    plugins: ['ToolsetRegistry'],
    costLatency: 'interactive',
    exampleInput: { action: 'call_editor_tool_destructive', toolName: 'GameplayTagsToolset.GameplayTagsToolset.RemoveTag', input: { TagName: 'Character.State.Dead' } },
    exampleOutput: { success: true, toolName: 'GameplayTagsToolset.GameplayTagsToolset.RemoveTag' },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'open_editor_tab', dispatchAction: 'control_editor',
    domain: D, family: F,
    summary: 'Open a registered editor tab by id via FGlobalTabmanager, the same path the Window menu uses. Content-source plugins register their windows globally — Bridge as "BridgeTab" — so this reaches them without depending on either plugin. Fab registers no fixed id (it numbers each tab Fab1, Fab2, ...), so tabId "Fab" opens a new Fab tab through the Fab browser API (UE 5.8+). This is also the correct way to authenticate against those services: each owns its own sign-in and persists its own session, so opening its window lets it log in on its own terms rather than reimplementing a login.',
    whenToUse: [
      'A Quixel/Fab capability reported NOT_AUTHENTICATED and the owning window must be opened so the user can sign in.',
      'An editor panel registered by a plugin needs to be brought up.',
    ],
    whenNotToUse: ['An asset editor should be opened for a specific asset (use open_asset).'],
    inputProps: {
      tabId: { type: 'string', description: 'Registered nomad tab id, for example "BridgeTab" (Quixel Bridge); "Fab" opens a new Fab tab.' },
    },
    outputProps: {
      tabId: { type: 'string', description: 'Tab id acted on.' },
      opened: { type: 'boolean', description: 'True when the tab manager returned a live tab.' },
    },
    required: ['tabId'],
    effect: 'write',
    costLatency: 'interactive',
    exampleInput: { action: 'open_editor_tab', tabId: 'BridgeTab' },
    exampleOutput: { success: true, opened: true },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'console_command', dispatchAction: 'console_command',
    domain: D, family: F,
    topics: ['console command', 'exec command', 'run command', 'stat fps', 'cheat command', 'type a cheat code'],
    summary: 'Execute an Unreal console command via cross-parent dispatch to the console_command bridge action.',
    whenToUse: ['A console command must be run from the editor context.'],
    whenNotToUse: ['A dedicated action exists for the operation.'],
    inputProps: { command: P.command },
    required: ['command'],
    effect: 'write',
   
    exampleInput: { action: 'console_command', command: 'r.SetRes 1920x1080' },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'restore_editor_window', domain: D, family: F,
    summary: 'Restore or minimize the main editor window without giving it focus. Restoring also stops the editor throttling itself '
      + 'while in the background (a minimized editor runs Play In Editor at about 3 fps, which makes every timed test lie); '
      + 'minimize puts the window away and turns the throttle back on, so an editor kept out of the way costs the machine next to nothing.',
    whenToUse: [
      'Play In Editor crawls (about 3 fps) because the editor window is minimized or in the background.',
      'The editor should stay out of the way while automation runs that needs no frames on screen: minimize it, and restore it for a timed run.',
    ],
    whenNotToUse: ['A screenshot or a sample_motion run is all that needs the window on screen (each restores a minimized editor by itself and minimizes it again afterwards).'],
    inputProps: {
      minimize: { type: 'boolean', description: 'Minimize the main editor window instead of restoring it, without taking focus, and turn Use Less CPU when in Background back on (saved). unthrottle is ignored. Default false.' },
      unthrottle: { type: 'boolean', description: 'Also turn off Use Less CPU when in Background (EditorPerformanceSettings.bThrottleCPUWhenNotForeground) for this editor session; it is not saved, so the next launch throttles again. Default true; ignored with minimize.' },
    },
    outputProps: {
      wasMinimized: { type: 'boolean', description: 'Whether the main window was minimized before the call.' },
      minimized: { type: 'boolean', description: 'Whether the window is minimized after the call, read back from the native window.' },
      restored: { type: 'boolean', description: 'Whether the window is on screen after the call.' },
      throttleOff: { type: 'boolean', description: 'Whether background CPU throttling is off after the call.' },
    },
    required: [],
    effect: 'write', behavior: { idempotency: 'idempotent' },
   
    exampleInput: { action: 'restore_editor_window', unthrottle: true },
    exampleOutput: { success: true, wasMinimized: true, restored: true, throttleOff: true },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'set_preferences', domain: D, family: F,
    summary: 'Set editor preferences for a category. Distinct from system_control set_project_setting.',
    whenToUse: ['Editor preferences must be configured for a category.'],
    whenNotToUse: ['Project settings are needed (use system_control set_project_setting).'],
    inputProps: { category: P.category, preferences: P.preferences },
    required: ['category', 'preferences'],
    effect: 'write', behavior: { idempotency: 'idempotent' },
   
    exampleInput: { action: 'set_preferences', category: 'Editor', preferences: { bUseSmallToolBarIcons: true } },
  }),
];
