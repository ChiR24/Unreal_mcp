/**
 * Room acoustics record: how a space sounds where a listener stands, from rays cast there.
 * Native Domains/Audio (McpAcoustics, McpAudioAcoustics).
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildCoreRecord } from '../core/builder.js';

const point = (description: string) => ({
  type: 'object', additionalProperties: false, required: ['x', 'y', 'z'],
  properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } }, description,
});
const call = (description: string) => ({ type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description });

export const ACOUSTICS_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'manage_audio', action: 'analyze_acoustics', dispatchAction: 'manage_audio', domain: 'audio', family: 'runtime',
    editorStates: ['edit', 'pie', 'simulate'],
    summary: 'Hear how a space sounds where a listener stands: 256 rays cast evenly in every direction from location (or actorName\'s location; the rays pass through that actor) give the room as heard from there: its volume and surface, mean free path, length, width and height along its own walls (proportion: the largest over the smallest), the share of rays that meet nothing within maxDistance (default 5000 cm: open sky, a large doorway), the delay of the first reflection, and its reverberation time rt60, the seconds sound takes to fall 60 dB. rt60 comes from a traced decay (each ray reflected on, 30% of the reflections scattered, losing absorption of its energy at each, read off its Schroeder curve), so it holds in corridors, flat halls and rooms with openings; eyringRt60 gives Eyring\'s formula beside it only for a closed room no more than 3 times as long as its smallest side, where that formula holds. absorption (default 0.1) is the share of sound the surfaces soak up: bare concrete or tile about 0.02-0.05, plaster or wood about 0.1, a furnished room 0.2-0.3, a treated studio 0.5 and up. Rays trace the visibility channel against complex collision, as the engine\'s sound occlusion does. When half the rays escape or more, it is outdoors or very open and no reverb is suggested; otherwise nextCall makes a Reverb Effect with that decay (RE_Room in /Game/Audio/Effects) and zoneCall places it as a reverb zone over the room\'s box (roomCenter, roomSize; rename both together). Reads only. Refused with POINT_IN_SOLID (the listener is inside geometry), ACTOR_NOT_FOUND, INVALID_ARGUMENT (neither location nor actorName).',
    topics: ['room acoustics', 'reverberation time', 'rt60', 'how does this room sound', 'echo in a room', 'reverb for this room', 'how much reverb', 'acoustics of a space'],
    whenToUse: [
      'Choosing a reverb that fits a room, a hall, a corridor or a cave instead of guessing its decay time.',
      'Checking whether a space is closed, long or open before placing audio volumes.',
    ],
    whenNotToUse: [
      'Making or placing a reverb with known settings: use create_audio_asset kind reverb_effect and create_audio_actor kind reverb_zone.',
      'How loud a sound is: use measure_loudness.',
    ],
    inputProps: {
      location: point('Where the listener stands, in cm: a point in the open air of the room.'),
      actorName: { type: 'string', description: 'An actor whose location is the listener, instead of location (a character, a sound source); the rays pass through it.' },
      absorption: { type: 'number', minimum: 0.02, maximum: 0.9, description: 'The share of sound energy the surfaces absorb at each reflection (default 0.1): 0.02-0.05 bare concrete or tile, 0.1 plaster or wood, 0.2-0.3 a furnished room, 0.5 and up heavy curtains or a treated studio.' },
      maxDistance: { type: 'number', minimum: 500, maximum: 20000, description: 'How far a ray reaches, in cm (default 5000); a ray that meets nothing within it counts as open.' },
    },
    effect: 'read',
    costLatency: 'interactive',
    exampleInput: { action: 'analyze_acoustics', location: { x: 300, y: 250, z: 150 } },
    exampleOutput: {
      success: true, listener: { x: 300, y: 250, z: 150 }, openShare: 0, volumeM3: 89.9, surfaceM2: 127.1, meanFreePathM: 2.83, firstReflectionMs: 8.8,
      lengthM: 6, widthM: 5, heightM: 3, proportion: 2, roomCenter: { x: 300, y: 250, z: 150 }, roomSize: { x: 600, y: 500, z: 300 }, rt60: 1.1, eyringRt60: 1.08,
      nextCall: { operation: 'execute', tool: 'manage_audio', action: 'create_reverb_effect', params: { name: 'RE_Room', decayTime: 1.1 } },
      zoneCall: { operation: 'execute', tool: 'manage_audio', action: 'create_reverb_zone', params: { name: 'RoomReverb', location: { x: 300, y: 250, z: 150 }, size: { x: 600, y: 500, z: 300 }, reverbEffect: '/Game/Audio/Effects/RE_Room' } },
      message: 'A room of about 90 m3 (6.0 x 5.0 x 3.0 m) at absorption 0.10: sound takes 1.10 s to fall 60 dB (Eyring: 1.08 s). nextCall makes a Reverb Effect with that decay; zoneCall then places it as a reverb zone over the room.',
    },
    outputProps: {
      listener: point('Where the rays were cast from.'),
      openShare: { type: 'number', description: 'The share of rays (0-1) that met nothing within maxDistance.' },
      volumeM3: { type: 'number', description: 'The volume seen from the listener, in cubic metres (a space around a corner is not counted).' },
      surfaceM2: { type: 'number', description: 'The surface seen from the listener, in square metres.' },
      meanFreePathM: { type: 'number', description: 'The mean distance sound travels between reflections (4 V / S), in metres.' },
      firstReflectionMs: { type: 'number', description: 'Out to the nearest surface and back, in milliseconds.' },
      lengthM: { type: 'number', description: 'The longer of the room\'s floor sides, along its own walls, in metres.' },
      widthM: { type: 'number', description: 'The shorter of its floor sides, in metres.' },
      heightM: { type: 'number', description: 'Floor to ceiling, in metres.' },
      proportion: { type: 'number', description: 'Its largest side over its smallest; past 3 it is long or flat and Eyring\'s formula does not hold.' },
      roomCenter: point('The middle of the box around every first hit (world axes), in cm.'),
      roomSize: point('The size of that box, in cm.'),
      rt60: { type: 'number', description: 'Seconds for sound to fall 60 dB, from the traced decay; absent when the decay fell less than 25 dB (most rays escaped).' },
      eyringRt60: { type: 'number', description: 'Eyring\'s formula, given only for a closed room with proportion under 3.' },
      nextCall: call('The create_reverb_effect call making a Reverb Effect with decayTime rt60 (0.1-20 s, the engine\'s range).'),
      zoneCall: call('The create_reverb_zone call placing that effect as an Audio Volume over roomCenter and roomSize.'),
    },
    outputRequired: ['openShare'],
  }),
];
