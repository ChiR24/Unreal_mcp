/**
 * Level operations: level io (2 actions). The terms of the shared
 * normalization rationale live with the D/NR constants in
 * operations-power.data.ts.
 *
 * Grounded in src/tools/handlers/level/runtime/level-asset-handlers.ts:
 * export_level/import_level route through manage_level.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildCoreRecord } from '../core/builder.js';
import { P } from './properties.js';
import { D, NR } from './operations-power.data.js';

export const OPERATIONS_IO_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'manage_level', action: 'export_level', dispatchAction: 'export_level',
    domain: D, family: 'io',
    summary: 'Export a level asset to a file.',
    whenToUse: ['A level must be serialized to an external file.'],
    whenNotToUse: ['The level should be saved as a new asset; use save_as.'],
    inputProps: { levelPath: P.levelPath, exportPath: P.exportPath, destinationPath: P.destinationPath },
    required: ['levelPath'],
    effect: 'read', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'export_level', levelPath: '/Game/Maps/Demo', exportPath: '/Temp/Demo.export' },
    exampleOutput: { success: true, message: 'Level exported' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'import_level', dispatchAction: 'import_level',
    domain: D, family: 'io',
    summary: 'Import a level asset from a package or source file.',
    whenToUse: ['A level must be imported from an external file.'],
    whenNotToUse: ['A new empty level is needed; use create_level.'],
    inputProps: {
      packagePath: P.packagePath, sourcePath: P.sourcePath,
      destinationPath: P.destinationPath, targetPath: P.targetPath,
      overwrite: P.overwrite,
    },
    required: ['packagePath'],
    effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'import_level', packagePath: '/Temp/Demo.export', destinationPath: '/Game/Maps/Imported' },
    exampleOutput: { success: true, message: 'Level imported' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
];
