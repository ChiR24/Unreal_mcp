/**
 * Level lifecycle family records (11 actions).
 *
 * Grounded in:
 * - src/tools/handlers/level/runtime/level-handlers.ts (load/save/save_as
 *   sub-action translation, add_sublevel) and level-asset-handlers.ts
 *   (create_level routing to manage_level_structure, delete_level,
 *   rename_level->action 'rename', duplicate_level->action 'duplicate',
 *   validate_level).
 * - native Private/Domains/Level/McpAutomationBridge_LevelHandlers.cpp:
 *   manage_level sub-actions load/load_level, save/save_level,
 *   save_as/save_level_as all route through HandleLoadLevelAction /
 *   HandleSaveCurrentLevelAction / HandleSaveLevelAsAction; delete and
 *   delete_level both map to EffectiveAction 'delete_level';
 *   rename_level->'rename', duplicate_level->'duplicate'.
 *
 * create_level dispatches to the manage_level_structure bridge route
 * (dispatchMode 'action') because TS routes it there with subAction
 * 'create_level', not through the manage_level parent.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildCoreRecord } from '../core/builder.js';
import { P } from './properties.js';

const F = 'lifecycle';
const D = 'level';
const NR = 'Distinct manage_level lifecycle verb and target; no cross-tool duplicate.';

export const LIFECYCLE_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'manage_level', action: 'load', dispatchAction: 'load', domain: D, family: F,
    summary: 'Load a level into the editor, optionally in streaming mode.',
    whenToUse: ['A level must be opened or streamed into the current session.'],
    whenNotToUse: ['The level is already the current level.'],
    inputProps: { levelPath: P.levelPath, streaming: P.streaming, saveDirtyPackages: P.saveDirtyPackages },
    required: ['levelPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'load', levelPath: '/Game/Maps/Demo', saveDirtyPackages: true },
    exampleOutput: { success: true, message: 'Level loaded' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'load_level', dispatchAction: 'load', domain: D, family: F,
    summary: 'Load a level into the editor (alias of load).',
    whenToUse: ['A level must be opened using the load_level verb.'],
    whenNotToUse: ['Prefer the shorter load verb.'],
    inputProps: { levelPath: P.levelPath, streaming: P.streaming, saveDirtyPackages: P.saveDirtyPackages },
    required: ['levelPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'load_level', levelPath: '/Game/Maps/Demo' },
    exampleOutput: { success: true, message: 'Level loaded' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'save', dispatchAction: 'save', domain: D, family: F,
    summary: 'Save the current level, optionally to a target path.',
    whenToUse: ['The current level must be persisted.'],
    whenNotToUse: ['A new path is required; use save_as instead.'],
    inputProps: { levelPath: P.levelPath, savePath: P.savePath, levelName: P.levelName },
    required: [],
    effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'save', levelName: 'Demo' },
    exampleOutput: { success: true, message: 'Level saved' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'save_level', dispatchAction: 'save', domain: D, family: F,
    summary: 'Save the current level (alias of save).',
    whenToUse: ['The current level must be saved using the save_level verb.'],
    whenNotToUse: ['Prefer the shorter save verb.'],
    inputProps: { levelPath: P.levelPath, savePath: P.savePath, levelName: P.levelName },
    required: [],
    effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'save_level', levelName: 'Demo' },
    exampleOutput: { success: true, message: 'Level saved' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'save_as', dispatchAction: 'save_level_as', domain: D, family: F,
    summary: 'Save the current level to a new path.',
    whenToUse: ['The current level must be saved to a new asset path.'],
    whenNotToUse: ['No destination path is available.'],
    inputProps: { savePath: P.savePath, destinationPath: P.destinationPath, levelPath: P.levelPath },
    required: ['savePath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'save_as', savePath: '/Game/Maps/DemoCopy' },
    exampleOutput: { success: true, message: 'Level saved as /Game/Maps/DemoCopy' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'save_level_as', dispatchAction: 'save_level_as', domain: D, family: F,
    summary: 'Save the current level to a new path (alias of save_as).',
    whenToUse: ['The current level must be saved to a new path using the save_level_as verb.'],
    whenNotToUse: ['Prefer the shorter save_as verb.'],
    inputProps: { savePath: P.savePath, destinationPath: P.destinationPath, levelPath: P.levelPath },
    required: ['savePath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'save_level_as', savePath: '/Game/Maps/DemoCopy' },
    exampleOutput: { success: true, message: 'Level saved as /Game/Maps/DemoCopy' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'create_level', dispatchAction: 'manage_level_structure', dispatchMode: 'action',
    domain: D, family: F,
    summary: 'Create a new level asset and load it into the editor.',
    whenToUse: ['A brand-new level must be created and opened.'],
    whenNotToUse: ['An existing level should be loaded instead.'],
    inputProps: {
      levelName: P.levelName, levelPath: P.levelPath, savePath: P.savePath,
      useWorldPartition: P.useWorldPartition, saveDirtyPackages: P.saveDirtyPackages,
      template: P.template,
    },
    required: ['levelName'],
    effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'create_level', levelName: 'NewMap', useWorldPartition: false },
    exampleOutput: { success: true, message: 'Level created and loaded' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'delete', dispatchAction: 'delete_level', domain: D, family: F,
    summary: 'Delete one or more level assets from disk.',
    whenToUse: ['A level asset must be permanently removed.'],
    whenNotToUse: ['The level should be unloaded rather than deleted.'],
    inputProps: { levelPath: P.levelPath, levelPaths: P.levelPaths },
    required: ['levelPath'],
    effect: 'destructive', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'delete', levelPath: '/Game/Maps/OldDemo' },
    exampleOutput: { success: true, message: 'Level deleted', deletedCount: 1 },
    outputProps: { deletedCount: { type: 'number', description: 'Number of levels successfully deleted.' } },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'delete_level', dispatchAction: 'delete_level', domain: D, family: F,
    summary: 'Delete a level asset from disk (alias of delete).',
    whenToUse: ['A level asset must be permanently removed using the delete_level verb.'],
    whenNotToUse: ['Multiple levels must be deleted; use delete with levelPaths.'],
    inputProps: { levelPath: P.levelPath, levelPaths: P.levelPaths, path: P.path },
    required: ['levelPath'],
    effect: 'destructive', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'delete_level', levelPath: '/Game/Maps/OldDemo' },
    exampleOutput: { success: true, message: 'Level deleted', deletedCount: 1 },
    outputProps: { deletedCount: { type: 'number', description: 'Number of levels successfully deleted.' } },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'rename_level', dispatchAction: 'rename', domain: D, family: F,
    summary: 'Rename a level asset, keeping it in the same parent directory.',
    whenToUse: ['A level asset must be renamed in place.'],
    whenNotToUse: ['The level should be moved to a different directory; use duplicate plus delete.'],
    inputProps: { levelPath: P.levelPath, sourcePath: P.sourcePath, newName: P.newName, overwrite: P.overwrite },
    required: ['newName'],
    effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'rename_level', levelPath: '/Game/Maps/Demo', newName: 'NewDemo' },
    exampleOutput: { success: true, message: 'Level renamed to /Game/Maps/NewDemo' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'duplicate_level', dispatchAction: 'duplicate', domain: D, family: F,
    summary: 'Duplicate a level asset to a new destination path.',
    whenToUse: ['A level asset must be copied to a new path.'],
    whenNotToUse: ['No destination path is available.'],
    inputProps: { levelPath: P.levelPath, sourcePath: P.sourcePath, destinationPath: P.destinationPath, targetPath: P.targetPath, overwrite: P.overwrite },
    required: ['destinationPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'duplicate_level', sourcePath: '/Game/Maps/Demo', destinationPath: '/Game/Maps/DemoCopy' },
    exampleOutput: { success: true, message: 'Level duplicated to /Game/Maps/DemoCopy' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
];
