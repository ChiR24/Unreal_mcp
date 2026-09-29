/**
 * Level lifecycle family records (11 actions).
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildCoreRecord } from '../core/builder.js';
import { P } from './properties.js';

const F = 'lifecycle';
const D = 'level';

export const LIFECYCLE_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'manage_level', action: 'load', dispatchAction: 'load', domain: D, family: F,
    topics: ['open level', 'load map', 'open map', 'switch level', 'change level', 'reload level'],
    summary: 'Load a level into the editor, or with streaming=true stream it into the open level as a sub-level.',
    whenToUse: ['A level must be opened or streamed into the current session.'],
    whenNotToUse: ['The level is already the current level.'],
    inputProps: { levelPath: P.levelPath, streaming: P.streaming, saveDirtyPackages: P.saveDirtyPackages },
    required: ['levelPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'load', levelPath: '/Game/Maps/Demo', saveDirtyPackages: true },
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'save', dispatchAction: 'save', domain: D, family: F,
    topics: ['save level', 'save map', 'save current level', 'persist level'],
    summary: 'Save the current level, optionally to a target path.',
    whenToUse: ['The current level must be persisted.'],
    whenNotToUse: ['A new path is required; use save_as instead.'],
    // levelName used to sit here and was never read; the save always acts on
    // the open level, which levelPath can pin and savePath can redirect.
    inputProps: { levelPath: P.openLevelPath, savePath: { ...P.savePath, description: 'Save the open level to this /Game path instead of in place (a save-as).' } },
    required: [],
    effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive',
    exampleInput: { action: 'save', levelPath: '/Game/Maps/Demo' },
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'save_as', dispatchAction: 'save_level_as', domain: D, family: F,
    summary: 'Save the current level to a new path.',
    whenToUse: ['The current level must be saved to a new asset path.'],
    whenNotToUse: ['No destination path is available.'],
    inputProps: { savePath: P.savePath, levelPath: P.openLevelPath },
    required: ['savePath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive',
    exampleInput: { action: 'save_as', savePath: '/Game/Maps/DemoCopy' },
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'save_level_as', dispatchAction: 'save_level_as', domain: D, family: F,
    summary: 'Save the current level to a new path (alias of save_as).',
    whenToUse: ['The current level must be saved to a new path using the save_level_as verb.'],
    whenNotToUse: ['Prefer the shorter save_as verb.'],
    inputProps: { savePath: P.savePath, levelPath: P.openLevelPath },
    required: ['savePath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive',
    exampleInput: { action: 'save_level_as', savePath: '/Game/Maps/DemoCopy' },
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'create_level', dispatchAction: 'manage_level_structure',
    domain: D, family: F,
    topics: ['new level', 'new map', 'create map', 'empty level', 'make a new map'],
    summary: 'Create a new level asset and load it into the editor.',
    whenToUse: ['A brand-new level must be created and opened.'],
    whenNotToUse: ['An existing level should be loaded instead.'],
    inputProps: {
      // The handler reads levelPath (or its alias savePath) as the folder for
      // levelName; it never applied a template, so none is declared.
      levelName: P.levelName,
      levelPath: { ...P.levelPath, description: 'Folder the new level goes in (e.g. /Game/Maps, combined with levelName), or its full path; omitted, the level lands in /Game/Maps.' },
      savePath: { ...P.savePath, description: 'Alias of levelPath.' },
      useWorldPartition: P.useWorldPartition, saveDirtyPackages: P.saveDirtyPackages,
    },
    required: ['levelName'],
    effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'create_level', levelName: 'NewMap', useWorldPartition: false },
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'delete', dispatchAction: 'delete_level', domain: D, family: F,
    topics: ['delete level', 'delete map', 'remove level', 'delete sublevel', 'delete map file'],
    summary: 'Delete one or more level assets from disk.',
    whenToUse: ['A level asset must be permanently removed.'],
    whenNotToUse: ['The level should be unloaded rather than deleted.'],
    inputProps: { levelPath: P.levelPath, levelPaths: P.levelPaths },
    required: ['levelPath'],
    effect: 'destructive', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'delete', levelPath: '/Game/Maps/OldDemo' },
    exampleOutput: { success: true, message: 'Level deleted', deletedCount: 1 },
    outputProps: { deletedCount: { type: 'number', description: 'Number of levels successfully deleted.' } },
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
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'rename_level', dispatchAction: 'rename', domain: D, family: F,
    summary: 'Rename a level asset, keeping it in the same parent directory.',
    whenToUse: ['A level asset must be renamed in place.'],
    whenNotToUse: ['The level should be moved to a different directory; use duplicate plus delete.'],
    inputProps: {
      levelPath: P.levelPath, sourcePath: P.sourcePath, newName: P.newName, overwrite: P.overwrite,
      destinationPath: { ...P.destinationPath, description: 'Full destination package path; moves the level there instead of renaming it in place (newName is then ignored).' },
    },
    required: [],
    requiredOneOf: ['newName', 'destinationPath'],
    effect: 'write', costLatency: 'interactive',
    exampleInput: { action: 'rename_level', levelPath: '/Game/Maps/Demo', newName: 'NewDemo' },
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
  }),
];
