/**
 * Level-structure volume records, grounded in HandleManageVolumesAction
 * (Domains/Volume/McpAutomationBridge_VolumeHandlers.cpp).
 *
 * Creation is ONE record, `create_volume`, standing for the 17 volume classes
 * the native Volume domain spawns, which used to be 23 actions (17 create_* plus
 * the 6 add_* variants that attach to an actor when actorPath is present).
 * `volumeClass` selects the bridge action through routing.dispatchBy, and every
 * former name stays callable as a folded legacy pair whose pins supply the class
 * it implied; add_* falls back to create_* when no actorPath is given. The other
 * five records manage existing volumes (extent, bounds, properties, removal,
 * listing). All are editor-state 'edit'.
 */
import type { CapabilityRecordSource, JsonObject } from '../../model.js';
import { buildWorldRecord, type FoldedActionSpec } from './builder.js';
import { P } from './properties.js';

const F = 'volume';
const VOLUME_PRIORITY = { type: 'number', description: 'Priority where volumes overlap (PhysicsVolume, PostProcessVolume).' };
const T = 'manage_level_structure';

/**
 * Volume class -> bridge action. Where an add_* action exists it is the
 * target, because it attaches to actorPath when present and creates otherwise.
 */
const VOLUME_CLASS_ACTIONS = {
  TriggerVolume: 'add_trigger_volume',
  TriggerBox: 'create_trigger_box',
  TriggerSphere: 'create_trigger_sphere',
  TriggerCapsule: 'create_trigger_capsule',
  BlockingVolume: 'add_blocking_volume',
  KillZVolume: 'add_kill_z_volume',
  PainCausingVolume: 'create_pain_causing_volume',
  PhysicsVolume: 'add_physics_volume',
  AudioVolume: 'create_audio_volume',
  ReverbVolume: 'create_reverb_volume',
  CullDistanceVolume: 'add_cull_distance_volume',
  PrecomputedVisibilityVolume: 'create_precomputed_visibility_volume',
  LightmassImportanceVolume: 'create_lightmass_importance_volume',
  NavMeshBoundsVolume: 'create_nav_mesh_bounds_volume',
  NavModifierVolume: 'create_nav_modifier_volume',
  CameraBlockingVolume: 'create_camera_blocking_volume',
  PostProcessVolume: 'add_post_process_volume',
} as const;

/** Every former action name, pinned to the class it implied. */
const FOLDED: readonly FoldedActionSpec[] = [
  { action: 'create_trigger_volume', pins: { volumeClass: 'TriggerVolume' } },
  { action: 'add_trigger_volume', pins: { volumeClass: 'TriggerVolume' } },
  { action: 'create_trigger_box', pins: { volumeClass: 'TriggerBox' } },
  { action: 'create_trigger_sphere', pins: { volumeClass: 'TriggerSphere' } },
  { action: 'create_trigger_capsule', pins: { volumeClass: 'TriggerCapsule' } },
  { action: 'create_blocking_volume', pins: { volumeClass: 'BlockingVolume' } },
  { action: 'add_blocking_volume', pins: { volumeClass: 'BlockingVolume' } },
  { action: 'create_kill_z_volume', pins: { volumeClass: 'KillZVolume' } },
  { action: 'add_kill_z_volume', pins: { volumeClass: 'KillZVolume' } },
  { action: 'create_pain_causing_volume', pins: { volumeClass: 'PainCausingVolume' } },
  { action: 'create_physics_volume', pins: { volumeClass: 'PhysicsVolume' } },
  { action: 'add_physics_volume', pins: { volumeClass: 'PhysicsVolume' } },
  { action: 'create_audio_volume', pins: { volumeClass: 'AudioVolume' } },
  { action: 'create_reverb_volume', pins: { volumeClass: 'ReverbVolume' } },
  { action: 'create_cull_distance_volume', pins: { volumeClass: 'CullDistanceVolume' } },
  { action: 'add_cull_distance_volume', pins: { volumeClass: 'CullDistanceVolume' } },
  { action: 'create_precomputed_visibility_volume', pins: { volumeClass: 'PrecomputedVisibilityVolume' } },
  { action: 'create_lightmass_importance_volume', pins: { volumeClass: 'LightmassImportanceVolume' } },
  { action: 'create_nav_mesh_bounds_volume', pins: { volumeClass: 'NavMeshBoundsVolume' } },
  { action: 'create_nav_modifier_volume', pins: { volumeClass: 'NavModifierVolume' } },
  { action: 'create_camera_blocking_volume', pins: { volumeClass: 'CameraBlockingVolume' } },
  { action: 'create_post_process_volume', pins: { volumeClass: 'PostProcessVolume' } },
  { action: 'add_post_process_volume', pins: { volumeClass: 'PostProcessVolume' } },
];

/**
 * Old names advertised as aliases, so a caller who types one still finds this
 * record on both doors. The three trigger shapes are left out: their words are
 * geometry primitives ("box", "sphere", "capsule") and an alias hit on either
 * door's word matcher outranked manage_geometry.create_box for "create box
 * mesh". They stay callable as folded {tool, action} pairs and reachable by the
 * `volumeClass` selector, but a FORMER CAPABILITY ID for them does not resolve.
 * tests/unit/gateway-dispatch-by.test.ts pins both halves of that split.
 */
const SHAPE_NAMES = new Set(['create_trigger_box', 'create_trigger_sphere', 'create_trigger_capsule']);
const ADVERTISED_ALIASES = FOLDED.filter((entry) => !SHAPE_NAMES.has(entry.action)).map((entry) => `${T}.${entry.action}`);

const volumeClass: JsonObject = {
  type: 'string',
  enum: Object.keys(VOLUME_CLASS_ACTIONS),
  description: 'Volume class to spawn. Each class takes only its own parameters; one it would ignore is refused.',
};

const actorPath: JsonObject = {
  type: 'string',
  description:
    'Attach the new volume to this actor instead of placing it standalone. Honoured for TriggerVolume, '
    + 'BlockingVolume, KillZVolume, PhysicsVolume, CullDistanceVolume and PostProcessVolume.',
};

export const LEVEL_VOLUME_RECORDS: readonly CapabilityRecordSource[] = [
  buildWorldRecord({
    parentTool: T, action: 'create_volume', dispatchAction: 'create_volume',
    folded: FOLDED,
    dispatchBy: { param: 'volumeClass', actions: VOLUME_CLASS_ACTIONS },
    aliases: ADVERTISED_ALIASES,
    topics: ['create volume', 'trigger volume', 'trigger box', 'trigger sphere', 'trigger capsule', 'blocking volume', 'kill z volume', 'post process volume'],
    family: F,
    summary: 'Create a volume actor of the chosen class in the level, or attach it to an actor when actorPath is given.',
    whenToUse: [
      'Any volume actor must be placed: trigger, blocking, kill-Z, pain, physics, audio, reverb, cull distance, '
      + 'precomputed visibility, Lightmass importance, nav bounds, nav modifier, camera blocking or post-process.',
    ],
    whenNotToUse: ['An existing volume must be resized or reconfigured; use set_volume_extent or set_volume_properties.'],
    inputProps: {
      volumeClass,
      volumeName: P.volumeName, location: P.location, rotation: P.rotation, extent: P.extent, actorPath,
      boxExtent: P.boxExtent, sphereRadius: P.sphereRadius, capsuleRadius: P.capsuleRadius, capsuleHalfHeight: P.capsuleHalfHeight,
      bPainCausing: P.bPainCausing, damagePerSec: P.damagePerSec,
      fluidFriction: P.fluidFriction, terminalVelocity: P.terminalVelocity, bWaterVolume: P.bWaterVolume,
      bEnabled: P.bEnabled, reverbVolume: P.reverbVolume, fadeTime: P.fadeTime,
      cullDistances: P.cullDistances,
      bUnbound: P.bUnbound, blendRadius: P.blendRadius, blendWeight: P.blendWeight,
      priority: VOLUME_PRIORITY,
      killZHeight: { type: 'number', description: 'World Z of the kill plane (KillZVolume); sets the volume height.' },
      postProcessSettings: {
        type: 'object', additionalProperties: false,
        description: 'PostProcessVolume overrides: bloomEnabled, exposureBias, vignetteIntensity, saturation, contrast, gamma.',
        properties: { bloomEnabled: { type: 'boolean' }, exposureBias: { type: 'number' }, vignetteIntensity: { type: 'number' }, saturation: { type: 'number' }, contrast: { type: 'number' }, gamma: { type: 'number' } },
      },
      save: P.levelEditSave,
    },
    required: ['volumeClass', 'location'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_volume', volumeClass: 'TriggerVolume', location: { x: 0, y: 0, z: 0 }, extent: { x: 500, y: 500, z: 200 } },
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'set_volume_extent', dispatchAction: 'set_volume_extent',
    family: F, summary: 'Set the extent (half-size) of an existing volume.',
    whenToUse: ['A volume must be resized.'], whenNotToUse: ['A new volume must be created; use create_volume.'],
    inputProps: { volumeName: P.volumeName, extent: P.extent, save: P.levelEditSave },
    required: ['volumeName', 'extent'], effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'set_volume_extent', volumeName: 'PP_01', extent: { x: 1000, y: 1000, z: 500 } },
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'set_volume_bounds', dispatchAction: 'set_volume_bounds',
    family: F, summary: 'Set the bounds (min/max) of an existing volume.',
    whenToUse: ['A volume must be repositioned/resized via min/max bounds.'], whenNotToUse: ['Only the extent must change; use set_volume_extent.'],
    inputProps: { volumeName: P.volumeName, bounds: P.boundsArray, save: P.levelEditSave },
    required: ['volumeName', 'bounds'], effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'set_volume_bounds', volumeName: 'PP_01', bounds: [-1000, -1000, -500, 1000, 1000, 500] },
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'set_volume_properties', dispatchAction: 'set_volume_properties',
    family: F, summary: 'Set generic properties (enabled, blend, priority) on an existing volume.',
    whenToUse: ['Volume properties must be updated without recreating it.'], whenNotToUse: ['The volume extent must change; use set_volume_extent.'],
    inputProps: { volumeName: P.volumeName, bEnabled: P.bEnabled, priority: VOLUME_PRIORITY, blendWeight: P.blendWeight, bWaterVolume: P.bWaterVolume, fluidFriction: P.fluidFriction, terminalVelocity: P.terminalVelocity, bPainCausing: P.bPainCausing, damagePerSec: P.damagePerSec, reverbVolume: P.reverbVolume, fadeTime: P.fadeTime, save: P.levelEditSave },
    required: ['volumeName'], effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'set_volume_properties', volumeName: 'PP_01', bEnabled: true },
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'remove_volume', dispatchAction: 'remove_volume',
    family: F, summary: 'Remove a volume actor from the level.',
    whenToUse: ['A volume actor must be permanently removed.'], whenNotToUse: ['The volume should be disabled; use set_volume_properties.'],
    inputProps: { volumeName: P.volumeName, save: P.levelEditSave },
    required: ['volumeName'], effect: 'destructive', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'remove_volume', volumeName: 'PP_01' },
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'get_volumes_info', dispatchAction: 'get_volumes_info',
    topics: ['box extents', 'trigger actors'],
    family: F, summary: 'List the volumes and trigger actors in the level with name, class, location and box extent, and a total count; filter by label or volumeType (a class name part such as Trigger or PostProcess).',
    whenToUse: ['The set of volumes must be enumerated or inspected.'], whenNotToUse: ['A single volume must be resized; use set_volume_extent.'],
    inputProps: { filter: P.filter, volumeType: P.volumeType },
    required: [], effect: 'read', costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'get_volumes_info', filter: 'Trigger' },
    exampleOutput: { success: true, message: 'Found 1 volumes/triggers', volumesInfo: { totalCount: 1, volumes: [{ name: 'Trigger_01', class: 'TriggerBox', location: { x: 0, y: 0, z: 100 }, extent: { x: 64, y: 64, z: 64 } }] } },
    outputProps: { volumesInfo: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'totalCount and volumes: each volume or trigger actor with name (its label), class, location and extent (the bounds half-size), as {x,y,z}.' } },
  }),
];
