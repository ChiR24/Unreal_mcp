/**
 * Level operations: level io (2 actions). The terms of the shared
 * normalization rationale live with the D/NR constants in
 * operations-power.data.ts.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildCoreRecord } from '../core/builder.js';
import { P } from './properties.js';
import { D } from './operations-power.data.js';

export const OPERATIONS_IO_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'manage_level', action: 'export_level', dispatchAction: 'export_level',
    domain: D, family: 'io',
    summary: 'Export the open level: as a .t3d text file inside the project folder (exportPath ending .t3d, relative to the project, e.g. Saved/Exports/Demo.t3d), or as a copy saved to another /Game map path. levelPath, when given, must be the open level (LEVEL_NOT_LOADED otherwise); a /Game copy refuses a destination that already holds another level.',
    whenToUse: ['The open level must be written out as text (.t3d) for a diff or another tool, or copied to a second map.'],
    whenNotToUse: ['The level should be saved as a new asset; use save_as.', 'A level that is not open (open it first, or copy its files with import_level).'],
    inputProps: { levelPath: P.levelPath, exportPath: P.exportPath, destinationPath: P.destinationPath },
    required: [],
    requiredOneOf: ['exportPath', 'destinationPath'],
    // Both forms write: a file in the project, or a saved copy of the level under /Game.
    effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'export_level', exportPath: 'Saved/Exports/Demo.t3d' },
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'import_level', dispatchAction: 'import_level',
    domain: D, family: 'io',
    summary: 'Copy a level map from one /Game package path to another (sourcePath or packagePath to destinationPath or targetPath): the .umap, its built data and its external actor and object files, backing up and restoring the destination if a step fails. A destination that exists is refused unless overwrite is true, and a .t3d file cannot be imported (NOT_SUPPORTED). The copy is not opened.',
    whenToUse: ['A level must be duplicated to another map path, files and all, without opening it.'],
    whenNotToUse: ['A new empty level is needed; use create_level.', 'The open level should be saved under a second name (use manage_level save_as).'],
    inputProps: {
      packagePath: P.packagePath, sourcePath: P.sourcePath,
      destinationPath: P.destinationPath, targetPath: P.targetPath,
      overwrite: P.overwrite,
    },
    required: [],
    requiredOneOf: ['sourcePath', 'packagePath'],
    effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'import_level', sourcePath: '/Game/Maps/Demo', destinationPath: '/Game/Maps/Demo_Copy' },
  }),
];
