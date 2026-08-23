/**
 * Plugin management records (3): list_plugins, enable_plugin, disable_plugin.
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
import type { CapabilityRecordSource } from '../../index.js';
import { buildCoreRecord } from '../core/builder.js';
import type { CoreRecordSpec } from '../core/builder.js';

const PT = 'system_control';
const NC = 'C_SAME_VERB_DIFFERENT_TARGET' as const;

function buildPostMigration(spec: CoreRecordSpec): CapabilityRecordSource {
  const base = buildCoreRecord(spec);
  return { ...base, normalization: { ...base.normalization, provenance: 'post-migration' } };
}

export const PLUGIN_RECORDS: readonly CapabilityRecordSource[] = [
  buildPostMigration({
    parentTool: PT,
    action: 'list_plugins',
    domain: 'project',
    family: 'plugin',
    summary: 'List every plugin discovered by this editor with its enabled state, category, version and mounted content path.',
    whenToUse: [
      'A capability or migrated asset needs a plugin and its enabled state must be checked first.',
      'The mounted content root of a plugin is needed in order to list the assets it ships.',
    ],
    whenNotToUse: ['Project content is being enumerated (use manage_asset list).'],
    inputProps: {
      filter: { type: 'string', description: 'Case-sensitive substring matched against the plugin name and category.' },
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
    costResources: 'low',
    dispatchAction: 'system_control',
    dispatchMode: 'tool',
    exampleInput: { action: 'list_plugins', filter: 'Chaos' },
    exampleOutput: { success: true, pluginCount: 3 },
    normalizationClass: NC,
    normalizationRationale: 'Distinct read-only plugin inventory capability. Routes via the system_control fallback dispatch to the native HandleManagePlugins handler, which reads IPluginManager::GetDiscoveredPlugins.',
  }),
  buildPostMigration({
    parentTool: PT,
    action: 'enable_plugin',
    domain: 'project',
    family: 'plugin',
    summary: 'Enable a plugin for this project by writing it into the .uproject. The editor must be restarted before the plugin\'s modules load and its content mounts, so the response reports restartRequired.',
    whenToUse: [
      'Migrated content depends on a plugin that is installed but disabled (for example ChaosVehicles for the advanced vehicle template).',
    ],
    whenNotToUse: [
      'The plugin is not installed at all — list_plugins returns NOT_FOUND and this cannot download it.',
      'The effect is needed within the current session; this takes effect only after a restart.',
    ],
    inputProps: {
      pluginName: { type: 'string', description: 'Plugin name exactly as reported by list_plugins (the .uplugin name, not the friendly name).' },
    },
    outputProps: {
      pluginName: { type: 'string', description: 'Plugin acted on.' },
      enabled: { type: 'boolean', description: 'Enabled state now recorded in the .uproject.' },
      changed: { type: 'boolean', description: 'False when the plugin was already in the requested state and nothing was written.' },
      restartRequired: { type: 'boolean', description: 'True whenever the project file changed; modules and content mount only at startup.' },
    },
    required: ['pluginName'],
    effect: 'write',
    costLatency: 'interactive',
    costResources: 'low',
    dispatchAction: 'system_control',
    dispatchMode: 'tool',
    exampleInput: { action: 'enable_plugin', pluginName: 'ChaosVehiclesPlugin' },
    exampleOutput: { success: true, enabled: true, restartRequired: true },
    normalizationClass: NC,
    normalizationRationale: 'Distinct project-descriptor write capability. Routes via the system_control fallback dispatch to the native HandleManagePlugins handler, which calls IProjectManager::SetPluginEnabled and SaveCurrentProjectToDisk.',
  }),
  buildPostMigration({
    parentTool: PT,
    action: 'disable_plugin',
    domain: 'project',
    family: 'plugin',
    summary: 'Disable a plugin for this project by removing it from the .uproject. Assets that depend on it fail to load after the restart, so confirm dependents first.',
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
    costResources: 'low',
    dispatchAction: 'system_control',
    dispatchMode: 'tool',
    exampleInput: { action: 'disable_plugin', pluginName: 'ChaosVehiclesPlugin' },
    exampleOutput: { success: true, enabled: false, restartRequired: true },
    normalizationClass: NC,
    normalizationRationale: 'Distinct project-descriptor write capability, the inverse of enable_plugin. Routes via the system_control fallback dispatch to the native HandleManagePlugins handler.',
  }),
];
