/**
 * Level-structure volume creation: ONE record, `create_volume`, stands for the
 * 17 volume classes the native Volume domain spawns, which used to be 23
 * actions (17 create_* plus the 6 add_* variants that attach to an actor when
 * actorPath is present). `volumeClass` selects the bridge action through
 * routing.dispatchBy, and every former name stays callable as a folded legacy
 * pair whose pins supply the class it implied. The management actions (extent,
 * bounds, properties, remove, info) live in volume-b. Grounded in
 * HandleManageVolumesAction (Domains/Volume/McpAutomationBridge_VolumeHandlers.cpp),
 * whose add_* branches fall back to create_* when no actorPath is given.
 */
import type { CapabilityRecordSource, JsonObject } from '../../index.js';
import { buildWorldRecord, type FoldedActionSpec } from './builder.js';
import { P } from './properties.js';

const F = 'volume';
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
  description: 'Volume class to spawn.',
};

const actorPath: JsonObject = {
  type: 'string',
  description:
    'Attach the new volume to this actor instead of placing it standalone. Honoured for TriggerVolume, '
    + 'BlockingVolume, KillZVolume, PhysicsVolume, CullDistanceVolume and PostProcessVolume.',
};

export const LEVEL_VOLUME_A_RECORDS: readonly CapabilityRecordSource[] = [
  buildWorldRecord({
    parentTool: T, action: 'create_volume', dispatchAction: 'create_volume',
    primaryProvenance: 'post-migration',
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
      save: P.save,
    },
    required: ['volumeClass', 'location'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_volume', volumeClass: 'TriggerVolume', location: { x: 0, y: 0, z: 0 }, extent: { x: 500, y: 500, z: 200 } },
    exampleOutput: { success: true, message: 'Trigger volume created' },
    normalizationRationale:
      'One operation for the volume family: volumeClass selects the native handler and every former name stays callable as a folded pair.',
  }),
];
