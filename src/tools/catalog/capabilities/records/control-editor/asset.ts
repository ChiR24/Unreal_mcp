/**
 * Asset and level navigation records: open_asset, close_asset, open_level,
 * focus_actor, save_all.
 *
 * Grounded in src/tools/handlers/editor/editor-asset-actions.ts and
 * editor-viewport-actions.ts (focus_actor). open_asset is read-effect
 * navigation; close_asset, open_level, save_all are write-effect. focus_actor
 * is read-effect viewport navigation. TS normalizes focus_actor to 'focus'
 * for handler routing but dispatches 'focus_actor' to the bridge.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildCoreRecord } from '../core/builder.js';
import { P } from './properties.js';

const F = 'asset';
const D = 'editor';
const NR = 'Distinct control_editor asset or level navigation operation with unique target semantics.';

export const ASSET_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'control_editor', action: 'open_asset', domain: D, family: F,
    summary: 'Open an asset in the appropriate editor by asset path.',
    whenToUse: ['An asset must be opened for editing or inspection.'],
    whenNotToUse: ['The asset is already open.'],
    inputProps: { assetPath: P.assetPath, path: P.path },
    required: ['assetPath'],
    effect: 'read',
    costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'open_asset', assetPath: '/Game/Materials/M_Base' },
    exampleOutput: { success: true, message: 'Asset opened' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'close_asset', domain: D, family: F,
    summary: 'Close an open asset editor by asset path.',
    whenToUse: ['An open asset editor must be closed.'],
    whenNotToUse: ['The asset is not open.'],
    inputProps: { assetPath: P.assetPath, path: P.path },
    required: ['assetPath'],
    effect: 'write',
    costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'close_asset', assetPath: '/Game/Materials/M_Base' },
    exampleOutput: { success: true, message: 'Asset closed' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'open_level', domain: D, family: F,
    summary: 'Open a level by asset path, loading it as the current level.',
    whenToUse: ['A different level must be loaded into the editor.'],
    whenNotToUse: ['The level is already loaded.'],
    inputProps: { levelPath: P.levelPath, path: P.path, assetPath: P.assetPath },
    required: ['levelPath'],
    effect: 'write',
    costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'open_level', levelPath: '/Game/Maps/EntryMap' },
    exampleOutput: { success: true, message: 'Level loaded' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'focus_actor', domain: D, family: F,
    summary: 'Focus the viewport camera on a specific actor by name.',
    whenToUse: ['The viewport must frame a specific actor.'],
    whenNotToUse: ['The actor does not exist in the current level.'],
    inputProps: { actorName: P.actorName, name: P.name },
    required: ['actorName'],
    effect: 'read',
    costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'focus_actor', actorName: 'BP_PlayerStart' },
    exampleOutput: { success: true, message: 'Focused on BP_PlayerStart' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
    normalizationRationale: 'TS normalizes focus_actor to focus for handler routing; bridge dispatches focus_actor. Distinct viewport navigation verb.',
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'save_all', domain: D, family: F,
    summary: 'Save all dirty assets and levels in the editor.',
    whenToUse: ['All unsaved changes must be persisted.'],
    whenNotToUse: ['Specific assets should be saved individually.'],
    inputProps: {},
    required: [],
    effect: 'write',
    costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'save_all' },
    exampleOutput: { success: true, message: 'All assets saved' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
];
