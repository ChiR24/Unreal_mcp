/**
 * manage_character records — part 3 of 3: MetaHuman Creator (UE 5.6+).
 *
 * Grounded in the native MetaHuman domain
 * (plugins/.../Private/Domains/MetaHuman/), which reaches MetaHuman through
 * UObject reflection rather than a link dependency — the plugin advertises UE
 * 5.0-5.8 and MetaHuman exists only on 5.6+, so on an older engine these
 * actions answer FEATURE_UNAVAILABLE instead of failing the build.
 *
 * Two prerequisites sit outside the editor and are the usual reason a build
 * fails: "MetaHuman Creator Core Data" is an Epic Games Launcher install
 * alongside the engine, and auto-rigging runs on an Epic cloud service that
 * needs the editor signed in. metahuman_status reports both up front rather
 * than leaving a caller to discover them one failed assembly at a time.
 */
import type { CapabilityRecordSource } from '../../../index.js';
import { buildRecord } from '../helpers.js';
import { P } from '../properties.js';
import { CHARACTER_P as C } from './character.props.js';

const T = 'manage_character';
const F = 'metahuman';

// Authored after the gateway migration, so the normalization audit skips these
// and the reviewed occurrence total stays where it was. See
// ../../../normalization/adjudicate.ts#REVIEWED_METRICS.
const POST = 'post-migration' as const;
const RATIONALE = 'MetaHuman Creator authoring, added after the gateway migration; no cross-tool duplicate.';

export const CHARACTER_3: readonly CapabilityRecordSource[] = [
  buildRecord({ parentTool: T, id: `${T}.metahuman_status`, action: 'metahuman_status', family: F,
    topics: ['metahuman', 'digital human', 'realistic character', 'metahuman ready', 'core data'],
    summary: 'Report MetaHuman readiness: plugin present, Core Data installed, and whether a character is rigged and buildable.',
    whenToUse: ['Before authoring or assembling a MetaHuman, to see every blocker at once.'],
    whenNotToUse: ['Use get_character_info for a gameplay Character Blueprint.'],
    inputProps: { action: P.action, characterPath: C.characterPath }, required: ['action'],
    effect: 'read', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    outputProps: { pluginAvailable: { type: 'boolean', description: 'MetaHuman Creator is loaded on this editor.' },
      coreDataInstalled: { type: 'boolean', description: 'MetaHuman Creator Core Data is installed next to the engine.' },
      ready: { type: 'boolean', description: 'No blockers remain.' },
      blockers: { type: 'array', items: { type: 'string' }, description: 'Every unmet prerequisite, each phrased as an action to take.' } },
    outputRequired: ['pluginAvailable', 'coreDataInstalled', 'ready', 'blockers'],
    exampleInput: { action: 'metahuman_status', characterPath: '/Game/MetaHumans/MH_Neo' },
    exampleOutput: { success: true, message: 'MetaHuman has 1 blocker(s).', pluginAvailable: true, coreDataInstalled: false, ready: false, blockers: ['MetaHuman Creator Core Data is not installed'] },
    normalizationProvenance: POST, normalizationRationale: RATIONALE }),

  buildRecord({ parentTool: T, id: `${T}.create_metahuman`, action: 'create_metahuman', family: F,
    topics: ['metahuman', 'create metahuman', 'digital human', 'photoreal character'],
    summary: 'Create a MetaHuman Character asset.',
    whenToUse: ['A photoreal human is needed and MetaHuman Creator is available.'],
    whenNotToUse: ['Use create_character_blueprint for a gameplay pawn.'],
    inputProps: { action: P.action, name: P.name, path: P.path }, required: ['action', 'name'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'medium',
    outputProps: { characterPath: C.characterPath, created: { type: 'boolean', description: 'False when the asset already existed.' } },
    outputRequired: ['characterPath', 'created'],
    exampleInput: { action: 'create_metahuman', name: 'MH_Neo', path: '/Game/MetaHumans' },
    exampleOutput: { success: true, message: 'MetaHuman character created', characterPath: '/Game/MetaHumans/MH_Neo', created: true },
    normalizationProvenance: POST, normalizationRationale: RATIONALE }),

  buildRecord({ parentTool: T, id: `${T}.rig_metahuman`, action: 'rig_metahuman', family: F,
    topics: ['metahuman', 'auto rig', 'face rig', 'rig character'],
    summary: 'Auto-rig a MetaHuman character so it can be assembled. Runs on an Epic cloud service.',
    whenToUse: ['A MetaHuman must be rigged before build_metahuman will accept it.'],
    whenNotToUse: ['The character is already rigged; check metahuman_status first.'],
    inputProps: { action: P.action, characterPath: C.characterPath, rigType: C.rigType, blocking: C.blocking, reportProgress: C.reportProgress },
    required: ['action', 'characterPath'],
    effect: 'write', behavior: { longRunning: true }, latency: 'long-running', resources: 'high',
    outputProps: { characterPath: C.characterPath, canBuild: { type: 'boolean', description: 'Whether the character is now buildable.' } },
    outputRequired: ['characterPath', 'canBuild'],
    exampleInput: { action: 'rig_metahuman', characterPath: '/Game/MetaHumans/MH_Neo', rigType: 'JointsAndBlendShapes' },
    exampleOutput: { success: true, message: 'MetaHuman rigged', characterPath: '/Game/MetaHumans/MH_Neo', canBuild: true },
    normalizationProvenance: POST, normalizationRationale: RATIONALE }),

  buildRecord({ parentTool: T, id: `${T}.build_metahuman`, action: 'build_metahuman', family: F,
    topics: ['metahuman', 'assemble metahuman', 'build metahuman', 'cinematic character'],
    summary: 'Run the MetaHuman assembly pipeline, producing the usable character assets.',
    whenToUse: ['A rigged MetaHuman must be turned into meshes, materials and a Blueprint.'],
    whenNotToUse: ['The character is not rigged yet; run rig_metahuman.'],
    inputProps: { action: P.action, characterPath: C.characterPath, pipelineType: C.pipelineType, pipelineQuality: C.pipelineQuality,
      buildPath: C.buildPath, commonFolderPath: C.commonFolderPath, nameOverride: C.nameOverride },
    required: ['action', 'characterPath'],
    effect: 'write', behavior: { longRunning: true }, latency: 'long-running', resources: 'high',
    outputProps: { characterPath: C.characterPath, pipelineType: C.pipelineType },
    outputRequired: ['characterPath'],
    exampleInput: { action: 'build_metahuman', characterPath: '/Game/MetaHumans/MH_Neo', pipelineType: 'Cinematic', buildPath: '/Game/MetaHumans' },
    exampleOutput: { success: true, message: 'MetaHuman assembled', characterPath: '/Game/MetaHumans/MH_Neo', pipelineType: 'Cinematic' },
    normalizationProvenance: POST, normalizationRationale: RATIONALE }),

  buildRecord({ parentTool: T, id: `${T}.export_metahuman`, action: 'export_metahuman', family: F,
    topics: ['metahuman', 'export metahuman', 'export skeletal mesh', 'export dna'],
    summary: 'Export a MetaHuman as skeletal meshes, material instances, or DNA assets.',
    whenToUse: ['The assembled character must become standalone assets for a shot or a DCC round-trip.'],
    whenNotToUse: ['Use build_metahuman to assemble it in the first place.'],
    inputProps: { action: P.action, characterPath: C.characterPath, exportType: C.exportType, projectPath: C.projectPath,
      externalPath: C.externalPath, headMesh: C.headMesh, bodyMesh: C.bodyMesh, fullBodyMesh: C.fullBodyMesh,
      dnaHead: C.dnaHead, dnaBody: C.dnaBody, applyAsOverrides: C.applyAsOverrides, overwrite: C.overwrite },
    required: ['action', 'characterPath'],
    effect: 'write', behavior: { longRunning: true }, latency: 'long-running', resources: 'high',
    outputProps: { characterPath: C.characterPath, exportType: C.exportType, projectPath: C.projectPath,
      assetsCreated: { type: 'number', description: 'Assets that appeared under projectPath. Zero means the export wrote nothing.' } },
    outputRequired: ['characterPath', 'exportType'],
    exampleInput: { action: 'export_metahuman', characterPath: '/Game/MetaHumans/MH_Neo', exportType: 'geometry', projectPath: '/Game/MetaHumans/Exported' },
    exampleOutput: { success: true, message: 'MetaHuman geometry exported', characterPath: '/Game/MetaHumans/MH_Neo', exportType: 'geometry', projectPath: '/Game/MetaHumans/Exported', assetsCreated: 2 },
    normalizationProvenance: POST, normalizationRationale: RATIONALE }),
];
