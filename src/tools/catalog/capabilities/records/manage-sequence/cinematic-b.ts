/**
 * Cinematic records part 2: configure_camera_rig_crane, add_fade_track,
 * add_level_visibility_track, add_material_parameter_track, add_particle_track,
 * add_skeletal_animation_track, add_transform_track, add_event_track,
 * add_property_track.
 *
 * Grounded in CINEMATICS_ACTIONS and native SequenceCinematics* bodies.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildRecord, P, SEQ_PLUGINS } from './helpers.js';

const F = 'cinematic';
const D = 'cinematics';
const NR = 'Distinct cinematic track operation with unique track type and target.';

function trackRecord(id: string, action: string, summary: string, extraProps: Record<string, unknown> = {}, required: string[] = ['action', 'path']): CapabilityRecordSource {
  return buildRecord({
    id, action, family: F, domain: D,
    summary,
    whenToUse: [`${summary}`],
    whenNotToUse: ['The track is not needed for this sequence.'],
    inputProps: { action: P.action, path: P.path, ...extraProps },
    required,
    effect: 'write', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action, path: '/Game/Cinematics/SEQ_Master' },
    exampleOutput: { success: true, message: 'Track added' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  });
}

export const CINEMATIC_RECORDS_B: readonly CapabilityRecordSource[] = [
  trackRecord('sequence.cinematic.configure_camera_rig_crane', 'configure_camera_rig_crane',
    'Configure a camera rig crane for boom camera movement.',
    { cranePitch: { type: 'number', description: 'Crane pitch in degrees.' }, craneYaw: { type: 'number', description: 'Crane yaw in degrees.' }, craneArmLength: { type: 'number', description: 'Crane arm length.' } }),
  trackRecord('sequence.cinematic.add_fade_track', 'add_fade_track',
    'Add a fade track for cinematic fade-in/fade-out transitions.'),
  trackRecord('sequence.cinematic.add_level_visibility_track', 'add_level_visibility_track',
    'Add a level visibility track to control level streaming during cinematic.',
    { levelNames: { type: 'array', items: P.property, description: 'Level names.' } }),
  trackRecord('sequence.cinematic.add_material_parameter_track', 'add_material_parameter_track',
    'Add a material parameter collection track to animate material parameters.'),
  trackRecord('sequence.cinematic.add_particle_track', 'add_particle_track',
    'Add a particle track to trigger particle systems during cinematic.'),
  trackRecord('sequence.cinematic.add_skeletal_animation_track', 'add_skeletal_animation_track',
    'Add a skeletal animation track to play animations on a skeletal mesh.',
    { animationSequencePath: { type: 'string', description: 'Animation sequence asset path.' }, skeletalMeshPath: { type: 'string', description: 'Skeletal mesh asset path.' } }),
  trackRecord('sequence.cinematic.add_transform_track', 'add_transform_track',
    'Add a transform track to animate actor transforms during cinematic.'),
  trackRecord('sequence.cinematic.add_event_track', 'add_event_track',
    'Add an event track to trigger events at specific frames.'),
  trackRecord('sequence.cinematic.add_property_track', 'add_property_track',
    'Add a property track to animate a specific property on a bound actor.',
    { property: P.property, actorName: P.actorName }, ['action', 'path', 'property']),
];
