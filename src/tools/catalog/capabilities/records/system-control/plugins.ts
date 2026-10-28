/**
 * Plugin management records (6): list_plugins, get_plugin_info, create_plugin, remove_plugin, enable_plugin, disable_plugin.
 *
 * Migrated content frequently depends on a plugin that ships with the engine
 * but is off in this project — the advanced vehicle template needs
 * ChaosVehicles, Bridge/Fab content needs the Bridge plugin. Without these the
 * assets copy in and then fail to load their classes, which reads as a broken
 * migration rather than a disabled module.
 *
 * All three route through the system_control fallback dispatch to the native
 * HandleManagePlugins handler, which edits the .uproject through
 * IProjectManager and saves it. Module loading and content mounting still
 * happen at startup, so the write reports restartRequired.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildCoreRecord } from '../core/builder.js';

const PT = 'system_control';

export const PLUGIN_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: PT,
    action: 'list_plugins',
    domain: 'project',
    family: 'plugin',
    topics: ['which are enabled', 'installed', 'content path'],
    summary: 'List every plugin discovered by this editor with its enabled state, category, version and mounted content path; enabledOnly and filter (name or category) narrow the list.',
    whenToUse: [
      'A capability or migrated asset needs a plugin and its enabled state must be checked first.',
      'The mounted content root of a plugin is needed in order to list the assets it ships.',
    ],
    whenNotToUse: ['Project content is being enumerated (use manage_asset list).'],
    inputProps: {
      filter: { type: 'string', description: 'Case-insensitive substring; a plugin is listed when its name or its category contains it.' },
      enabledOnly: { type: 'boolean', description: 'Return only plugins currently enabled for this project.' },
    },
    outputProps: {
      plugins: {
        type: 'array',
        description: 'Discovered plugins, name-sorted.',
        items: {
          type: 'object',
          additionalProperties: false,
          properties: {
            name: { type: 'string', description: 'Plugin name — the .uplugin file name, and the value enable_plugin expects.' },
            enabled: { type: 'boolean', description: 'Enabled for this project.' },
            canContainContent: { type: 'boolean', description: 'Plugin declares a Content directory.' },
            friendlyName: { type: 'string', description: 'Display name from the descriptor.' },
            category: { type: 'string', description: 'Descriptor category, as grouped in the Plugins browser.' },
            versionName: { type: 'string', description: 'Descriptor version string.' },
            mountedContentPath: { type: 'string', description: 'Mounted content root (for example /ChaosVehiclesPlugin), empty when the plugin ships no content. Pass to manage_asset list to enumerate what it provides.' },
          },
        },
      },
      pluginCount: { type: 'number', description: 'Number of plugins returned.' },
    },
    required: [],
    effect: 'read',
    costLatency: 'interactive',
    dispatchAction: 'system_control',
    exampleInput: { action: 'list_plugins', filter: 'Chaos' },
    exampleOutput: { success: true, pluginCount: 3 },
  }),
  buildCoreRecord({
    parentTool: PT,
    action: 'get_plugin_info',
    domain: 'project',
    family: 'plugin',
    topics: ['plugin details', 'plugin dependencies', 'what depends on this plugin', 'which plugin owns this asset', 'plugin modules', 'plugin version'],
    summary: 'Read one plugin from its descriptor: name, description, category, version, author, enabled state and its default, type (engine, project, ...), beta and experimental flags, mounted content path, its modules (type, loading phase), the plugins it needs (dependencies) and the installed plugins that need it (dependents, a disabled one marked). assetPath instead finds the plugin whose content holds that asset; one under /Game, /Engine or /Script is no plugin\'s and is refused PLUGIN_NOT_FOUND, as is a name no installed plugin has.',
    whenToUse: [
      'A plugin is about to be disabled and what would break must be known first (dependents).',
      'An asset path starts with a plugin mount (/SomePlugin/...) and its plugin, version or author is needed.',
      'Which modules or other plugins a plugin brings in must be known before enabling it.',
    ],
    whenNotToUse: ['Every plugin is to be listed or searched by name or category (use list_plugins).'],
    inputProps: {
      pluginName: { type: 'string', description: 'Plugin name as list_plugins reports it (the .uplugin name, not the friendly name).' },
      assetPath: { type: 'string', description: 'An asset or folder path (/SomePlugin/Meshes/SM_Rock): the plugin that mounts its first segment is read.' },
    },
    requiredOneOf: ['pluginName', 'assetPath'],
    outputProps: {
      pluginName: { type: 'string', description: 'The .uplugin name.' },
      friendlyName: { type: 'string', description: 'Display name.' },
      description: { type: 'string', description: 'Descriptor description.' },
      category: { type: 'string', description: 'Plugins browser category.' },
      versionName: { type: 'string', description: 'Version string.' },
      version: { type: 'number', description: 'Version number.' },
      createdBy: { type: 'string', description: 'Author.' },
      docsURL: { type: 'string', description: 'Documentation link, often empty.' },
      enabled: { type: 'boolean', description: 'Enabled for this project now.' },
      enabledByDefault: { type: 'string', enum: ['enabled', 'disabled', 'unspecified'], description: 'What the descriptor asks for when a project does not say.' },
      type: { type: 'string', enum: ['engine', 'enterprise', 'project', 'external', 'mod'], description: 'Where the plugin is installed; one create_plugin made this session reads external until the editor restarts.' },
      beta: { type: 'boolean', description: 'Marked beta.' },
      experimental: { type: 'boolean', description: 'Marked experimental.' },
      explicitlyLoaded: { type: 'boolean', description: 'Loads only when asked for (game feature plugins), not at startup.' },
      canContainContent: { type: 'boolean', description: 'Ships a Content folder.' },
      mountedContentPath: { type: 'string', description: 'Content root (/SomePlugin/), empty when it ships no content.' },
      modules: { type: 'array', items: { type: 'object', additionalProperties: false, properties: { name: { type: 'string' }, type: { type: 'string', description: 'Runtime, Editor, Developer, ...' }, loadingPhase: { type: 'string' } } }, description: 'Code modules the plugin declares.' },
      dependencies: { type: 'array', items: { type: 'object', additionalProperties: false, properties: { name: { type: 'string' }, enabled: { type: 'boolean' }, optional: { type: 'boolean' } } }, description: 'Plugins its descriptor lists.' },
      dependents: { type: 'array', items: { type: 'string' }, description: 'Installed plugins whose descriptor needs this one, a disabled one marked "(disabled)".' },
    },
    effect: 'read',
    dispatchAction: 'system_control',
    exampleInput: { action: 'get_plugin_info', pluginName: 'ChaosVehiclesPlugin' },
    exampleOutput: { success: true, pluginName: 'ChaosVehiclesPlugin', enabled: true, type: 'engine', dependents: [] },
  }),
  buildCoreRecord({
    parentTool: PT,
    action: 'create_plugin',
    domain: 'project',
    family: 'plugin',
    topics: ['new plugin', 'make a plugin', 'content plugin', 'content only plugin', 'modular content'],
    summary: 'Make a content-only plugin in this project\'s Plugins folder (the Plugins browser\'s Content Only template: a descriptor, a Content folder and the template icon), mount it and enable it in the project at once, so assets can go under its mountedContentPath (/Name/) straight away. A plugin with code is not made (it needs a build). A name taken by any installed plugin, or by a folder, is PLUGIN_EXISTS; a name that is no valid identifier is INVALID_ARGUMENT; nothing is created then.',
    whenToUse: [
      'Content should live in its own plugin (to share between projects, to ship separately, or to keep a feature\'s assets apart).',
    ],
    whenNotToUse: [
      'The plugin needs C++ code (create_cpp_class and a build are a different path).',
      'An installed plugin only needs turning on (use enable_plugin).',
    ],
    inputProps: {
      pluginName: { type: 'string', description: 'The plugin\'s name: its folder, its .uplugin and its content root /Name/. Letters, digits and underscores, starting with a letter.' },
      friendlyName: { type: 'string', description: 'Display name in the Plugins browser (default the name).' },
      description: { type: 'string', description: 'Descriptor description.' },
      category: { type: 'string', description: 'Plugins browser category (default Other).' },
      createdBy: { type: 'string', description: 'Author.' },
    },
    required: ['pluginName'],
    outputProps: {
      pluginName: { type: 'string', description: 'The plugin made.' },
      folder: { type: 'string', description: 'Its folder, relative to the project.' },
      mountedContentPath: { type: 'string', description: 'Its content root (/Name/), mounted now.' },
      enabled: { type: 'boolean', description: 'Enabled in the project.' },
    },
    effect: 'write',
    costLatency: 'interactive',
    dispatchAction: 'system_control',
    exampleInput: { action: 'create_plugin', pluginName: 'ForestKit', description: 'Trees and rocks for the forest levels.' },
    exampleOutput: { success: true, pluginName: 'ForestKit', mountedContentPath: '/ForestKit/', enabled: true },
  }),
  buildCoreRecord({
    parentTool: PT,
    action: 'remove_plugin',
    domain: 'project',
    family: 'plugin',
    topics: ['delete plugin', 'remove a plugin from the project'],
    summary: 'Take a plugin that no longer loads out of this project: its folder moves to Saved/EngineWire/RemovedPlugins/<name> (delete it there for good, or move it back to restore it) and the .uproject drops it. A plugin this editor has loaded is refused PLUGIN_LOADED (only a restart unloads one: disable_plugin it, restart the editor, then remove it). Only a plugin in this project\'s Plugins folder with no code is removed; an engine or marketplace plugin, or one with code (modules or a Source folder), is refused PLUGIN_NOT_REMOVABLE. A refusal changes nothing; MOVE_FAILED (a file in it is open) leaves the project as it was.',
    whenToUse: ['A content plugin made by mistake, or no longer wanted, should leave the project and the disk (after disable_plugin and a restart).'],
    whenNotToUse: [
      'The plugin should only stop loading (use disable_plugin: its files stay).',
      'Other content still uses its assets (they break).',
    ],
    inputProps: {
      pluginName: { type: 'string', description: 'The plugin to remove, as list_plugins names it.' },
    },
    required: ['pluginName'],
    outputProps: {
      pluginName: { type: 'string', description: 'The plugin removed.' },
      movedTo: { type: 'string', description: 'Where its folder is now, relative to the project.' },
      referenceRemoved: { type: 'boolean', description: 'True when the .uproject named it and no longer does.' },
    },
    effect: 'destructive',
    costLatency: 'interactive',
    dispatchAction: 'system_control',
    exampleInput: { action: 'remove_plugin', pluginName: 'ForestKit' },
    exampleOutput: { success: true, pluginName: 'ForestKit', movedTo: 'Saved/EngineWire/RemovedPlugins/ForestKit', referenceRemoved: true },
  }),
  buildCoreRecord({
    parentTool: PT,
    action: 'enable_plugin',
    domain: 'project',
    family: 'plugin',
    summary: 'Enable a plugin for this project by writing it into the .uproject, or limit it to some build targets (targets ["Editor"] keeps an editor-only plugin out of a packaged game). A plugin on only because another plugin needs it is written into the project file. A plugin the editor has not loaded yet needs a restart, so the response reports restartRequired.',
    whenToUse: [
      'Migrated content depends on a plugin that is installed but disabled (for example ChaosVehicles for the advanced vehicle template).',
      'A plugin used only in the editor should stay out of the packaged game (targets ["Editor"]), or one the game uses must stay on when the plugin that brought it in is limited to the editor.',
    ],
    whenNotToUse: [
      'The plugin is not installed at all — list_plugins returns NOT_FOUND and this cannot download it.',
      'The effect is needed within the current session; this takes effect only after a restart.',
    ],
    inputProps: {
      pluginName: { type: 'string', description: 'Plugin name exactly as reported by list_plugins (the .uplugin name, not the friendly name).' },
      targets: {
        type: 'array',
        items: { type: 'string', enum: ['Editor', 'Game', 'Client', 'Server', 'Program'] },
        description: 'Build targets the plugin is enabled for, every other build leaving it out: ["Editor"] keeps it in the editor and out of a packaged game. An empty list lifts the limit.',
      },
    },
    outputProps: {
      pluginName: { type: 'string', description: 'Plugin acted on.' },
      enabled: { type: 'boolean', description: 'Enabled state now recorded in the .uproject.' },
      targets: { type: 'array', items: { type: 'string' }, description: 'The build targets the plugin is now limited to; absent when it is enabled for every target.' },
      changed: { type: 'boolean', description: 'False when the plugin was already in the requested state and nothing was written.' },
      restartRequired: { type: 'boolean', description: 'True when this editor loads or drops the plugin only at its next start; a packaged game takes the project file as it is when it builds.' },
    },
    required: ['pluginName'],
    effect: 'write',
    costLatency: 'interactive',
    dispatchAction: 'system_control',
    exampleInput: { action: 'enable_plugin', pluginName: 'ChaosVehiclesPlugin' },
    exampleOutput: { success: true, enabled: true, restartRequired: true },
  }),
  buildCoreRecord({
    parentTool: PT,
    action: 'disable_plugin',
    domain: 'project',
    family: 'plugin',
    topics: ['turn off a plugin', 'stop a plugin loading'],
    summary: 'Disable a plugin for this project by removing it from the .uproject. Assets that depend on it fail to load after the restart, so confirm dependents first (get_plugin_info lists the plugins that need it).',
    whenToUse: ['A plugin enabled for a since-removed experiment should no longer load.'],
    whenNotToUse: ['Project content still references the plugin\'s classes or assets.'],
    inputProps: {
      pluginName: { type: 'string', description: 'Plugin name exactly as reported by list_plugins.' },
    },
    outputProps: {
      pluginName: { type: 'string', description: 'Plugin acted on.' },
      enabled: { type: 'boolean', description: 'Enabled state now recorded in the .uproject.' },
      changed: { type: 'boolean', description: 'False when the plugin was already disabled and nothing was written.' },
      restartRequired: { type: 'boolean', description: 'True whenever the project file changed.' },
    },
    required: ['pluginName'],
    effect: 'write',
    costLatency: 'interactive',
    dispatchAction: 'system_control',
    exampleInput: { action: 'disable_plugin', pluginName: 'ChaosVehiclesPlugin' },
    exampleOutput: { success: true, enabled: false, restartRequired: true },
  }),
];
