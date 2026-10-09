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
    // No standalone "load" here: the fold drops a line that names its own action.
    whenNotToUse: ['The level is already the open one: the call reloads nothing and keeps its unsaved changes (the reply says alreadyLoaded and reports the unsaved state); to drop them, open another level with discardUnsaved true, to keep them use save.',
      'Open levels have unsaved changes that opening another level would drop: the call is refused with DIRTY_PACKAGES, listing them in unsavedPackages, until saveDirtyPackages or discardUnsaved says what to do with them.'],
    inputProps: {
      levelPath: P.levelPath, streaming: P.streaming,
      saveDirtyPackages: { ...P.saveDirtyPackages, description: 'Save every dirty level and asset package first, in every mode (an interactive editor used to ignore it); the load fails DIRTY_PACKAGES when one cannot be saved, and a headless run refuses to load over dirty packages unless this is true. Done even when the level is already open and nothing is loaded.' },
      discardUnsaved: { type: 'boolean', description: 'Load even though open levels have unsaved changes, dropping them. Without this or saveDirtyPackages, a load that would drop unsaved level changes is refused with DIRTY_PACKAGES, which lists them (the editor loaded over them without a word).' },
    },
    required: ['levelPath'],
    outputProps: {
      alreadyLoaded: { type: 'boolean', description: 'True when levelPath was already the open level: nothing was loaded or reloaded, its unsaved changes were kept and a running Play In Editor session was left alone.' },
      reloaded: { type: 'boolean', description: 'Whether this call loaded the level from disk (false when alreadyLoaded).' },
      dirtyWorldPackagesBeforeLoad: { type: 'number', description: 'Level packages with unsaved changes when the call began, counted in every mode (it read 0 in an interactive editor).' },
      dirtyContentPackagesBeforeLoad: { type: 'number', description: 'Asset packages with unsaved changes when the call began.' },
      savedDirtyPackagesBeforeLoad: { type: 'boolean', description: 'Whether saveDirtyPackages saved every dirty package before the load.' },
      unsaved: { type: 'boolean', description: 'alreadyLoaded only: whether the open level (or one of its external actor packages) still has unsaved changes after any save the call made.' },
      unsavedPackages: { type: 'array', items: { type: 'string' }, description: 'alreadyLoaded only: the packages with unsaved changes (the first 100).' },
      unsavedPackageCount: { type: 'number', description: 'alreadyLoaded only: how many packages have unsaved changes.' },
      settledSeconds: { type: 'number', description: 'How long the reply waited for the level to settle (its water, streamed textures and effects drawn), so a screenshot right after shows the level as it looks.' }, texturesStillStreaming: { type: 'number', description: 'Textures still streaming in when the wait ended at its cap; absent when streaming had gone quiet.' },
    },
    outputRequired: [],
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
      useWorldPartition: P.useWorldPartition,
      saveDirtyPackages: { ...P.saveDirtyPackages, description: 'Save every dirty level and asset package first: the new level replaces the open one. Without it, open levels with unsaved changes refuse the call with DIRTY_PACKAGES (listing them in unsavedPackages) and nothing is created.' },
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
