/**
 * Transform and physics records: set_transform and its five aliases
 * (teleport_actor, set_actor_location, set_actor_rotation, set_actor_scale,
 * set_actor_transform), get_transform and its alias (get_actor_transform),
 * and apply_force.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildCoreRecord } from '../core/builder.js';
import { DOMAIN, P } from './properties.js';

const FAMILY_TRANSFORM = 'transform';
const FAMILY_PHYSICS = 'physics';

// Raising a 16-part castle by 600 took 16 absolute locations, each read first.
const OFFSET = {
  type: 'array', items: { type: 'number' }, minItems: 3, maxItems: 3,
  description: 'Move by [dx, dy, dz] from where the actor stands now, instead of to an absolute location; give one or the other.',
} as const;

const TRANSFORM_INPUT = {
  actorName: P.pieActorName,
  location: P.location,
  offset: OFFSET,
  rotation: P.rotation,
  scale: P.scale,
  actors: {
    type: 'array',
    items: {
      type: 'object',
      properties: { actorName: P.actorName, location: P.location, offset: OFFSET, rotation: P.rotation, scale: P.scale },
      required: ['actorName'],
      additionalProperties: false,
    },
    description: 'Many actors in one call, each {actorName, location? or offset?, rotation?, scale?} with its own values (an omitted part keeps its current value); every actor is reported, and the call fails naming any that did not move.',
  },
};

// The receipt is evidence, not an ack: the handler reads the transform back off
// the actor after writing it and reports that, so a caller can see an attached,
// simulating or otherwise constrained actor keeping its old orientation instead
// of being told "updated" and finding out at play time.
const TRANSFORM_OUTPUT = {
  success: true,
  message: 'Actor transform updated',
  actorName: 'Cube1',
  location: [10, 20, 30],
  rotation: [0, 90, 0],
  scale: [1, 1, 1],
};

// The example above is only honest if the schema admits the fields it shows.
// These four are what the handler writes back (ControlActor/..._ControlActorTransform.cpp
// sets actorName plus a read-back location/rotation/scale array on every reply),
// so declaring them keeps the receipt's evidence inside the contract instead of
// smuggling it through `details`.
const TRANSFORM_OUTPUT_PROPS = {
  actorName: { type: 'string', description: 'Label of the actor whose transform was written.' },
  location: {
    type: 'array', items: { type: 'number' },
    description: 'World location [x, y, z] read back off the actor after the write.',
  },
  rotation: {
    type: 'array', items: { type: 'number' },
    description: 'World rotation [pitch, yaw, roll] read back off the actor after the write.',
  },
  scale: {
    type: 'array', items: { type: 'number' },
    description: 'World scale [x, y, z] read back off the actor after the write.',
  },
  results: {
    type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true },
    description: 'actors: one entry per actor (actorName, success, read-back location/rotation/scale, placementWarning, error).',
  },
  movedActors: { type: 'number', description: 'actors: how many actors took their transform.' },
  affectedActors: {
    type: 'array', items: { type: 'string' },
    description: 'actors: the actors that took their transform, each by name and once; the receipt lists them as changes, with an actor handle each.',
  },
} as const;

export const TRANSFORM_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'set_transform',
    domain: DOMAIN,
    family: FAMILY_TRANSFORM,
    topics: ['move actor', 'set actor location', 'set position', 'rotate actor', 'set rotation', 'scale actor', 'teleport actor', 'translate'],
    aliases: ['control_actor.move', 'control_actor.move_actor'],
    summary: 'Move, rotate or scale an actor by setting its world transform (location, rotation, scale) or moving it by an offset, or many actors in one call with actors.',
    whenToUse: ['An actor must be moved, rotated, or rescaled in one call.', 'Several actors each need their own new location, rotation or scale (actors).', 'A group of actors must shift by the same amount (actors, each with offset).'],
    whenNotToUse: ['Only the transform needs to be read (use get_transform).'],
    inputProps: TRANSFORM_INPUT,
    required: [],
    requiredOneOf: ['actorName', 'actors'],
    effect: 'write',
    behavior: { idempotency: 'idempotent' },
    exampleInput: { action: 'set_transform', actorName: 'Cube1', location: [10, 20, 30], rotation: [0, 90, 0], scale: [1, 1, 1] },
    outputProps: TRANSFORM_OUTPUT_PROPS,
    exampleOutput: TRANSFORM_OUTPUT,
  }),
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'teleport_actor',
    domain: DOMAIN,
    family: FAMILY_TRANSFORM,
    summary: 'Alias of set_transform. The bridge dispatches both names to the same handler.',
    whenToUse: ['Preferred when callers use the teleport_actor verb.'],
    whenNotToUse: ['Use set_transform to avoid alias normalization.'],
    inputProps: TRANSFORM_INPUT,
    required: [],
    requiredOneOf: ['actorName', 'actors'],
    effect: 'write',
    behavior: { idempotency: 'idempotent' },
    exampleInput: { action: 'teleport_actor', actorName: 'Cube1', location: [5, 5, 5] },
    outputProps: TRANSFORM_OUTPUT_PROPS,
    exampleOutput: TRANSFORM_OUTPUT,
  }),
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'set_actor_location',
    domain: DOMAIN,
    family: FAMILY_TRANSFORM,
    summary: 'Alias of set_transform (location-only intent); normalizes to set_transform.',
    whenToUse: ['An actor must be moved to a new location.'],
    whenNotToUse: ['Use set_transform for the full transform verb.'],
    inputProps: TRANSFORM_INPUT,
    required: [],
    requiredOneOf: ['actorName', 'actors'],
    effect: 'write',
    behavior: { idempotency: 'idempotent' },
    exampleInput: { action: 'set_actor_location', actorName: 'Cube1', location: [100, 0, 0] },
    outputProps: TRANSFORM_OUTPUT_PROPS,
    exampleOutput: TRANSFORM_OUTPUT,
  }),
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'set_actor_rotation',
    domain: DOMAIN,
    family: FAMILY_TRANSFORM,
    summary: 'Alias of set_transform (rotation-only intent); normalizes to set_transform.',
    whenToUse: ['An actor must be rotated.'],
    whenNotToUse: ['Use set_transform for the full transform verb.'],
    inputProps: TRANSFORM_INPUT,
    required: [],
    requiredOneOf: ['actorName', 'actors'],
    effect: 'write',
    behavior: { idempotency: 'idempotent' },
    exampleInput: { action: 'set_actor_rotation', actorName: 'Cube1', rotation: [0, 45, 0] },
    outputProps: TRANSFORM_OUTPUT_PROPS,
    exampleOutput: TRANSFORM_OUTPUT,
  }),
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'set_actor_scale',
    domain: DOMAIN,
    family: FAMILY_TRANSFORM,
    summary: 'Alias of set_transform (scale-only intent); normalizes to set_transform.',
    whenToUse: ['An actor must be rescaled.'],
    whenNotToUse: ['Use set_transform for the full transform verb.'],
    inputProps: TRANSFORM_INPUT,
    required: [],
    requiredOneOf: ['actorName', 'actors'],
    effect: 'write',
    behavior: { idempotency: 'idempotent' },
    exampleInput: { action: 'set_actor_scale', actorName: 'Cube1', scale: [2, 2, 2] },
    outputProps: TRANSFORM_OUTPUT_PROPS,
    exampleOutput: TRANSFORM_OUTPUT,
  }),
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'get_transform',
    domain: DOMAIN,
    family: FAMILY_TRANSFORM,
    topics: ['actor location', 'actor position', 'actor rotation', 'where is the actor'],
    summary: 'Read the world transform (location, rotation, scale) of an actor.',
    whenToUse: ['The current transform of an actor must be inspected.'],
    whenNotToUse: ['The transform should be changed (use set_transform).'],
    inputProps: { actorName: P.pieActorName },
    required: ['actorName'],
    outputProps: { location: P.location, rotation: P.rotation, scale: P.scale },
    outputRequired: [],
    effect: 'read',
    exampleInput: { action: 'get_transform', actorName: 'Cube1' },
    exampleOutput: { success: true, message: 'Transform for Cube1', location: [0, 0, 0], rotation: [0, 0, 0], scale: [1, 1, 1] },
  }),
  // One call that watches an actor over GAME time in PIE. Proving a jump, a
  // spring launch or a patrol used to be a sleep-and-poll loop whose samples
  // landed wherever the editor's frame rate put them, so a death between two
  // polls read as a teleport back to the start.
  {
    ...buildCoreRecord({
      parentTool: 'control_actor',
      action: 'sample_motion',
      domain: DOMAIN,
      family: FAMILY_TRANSFORM,
      topics: ['sample motion', 'record trajectory', 'track actor over time', 'watch actor move', 'jump height'],
      summary: 'Watch an actor over game time in Play-In-Editor and return its location, velocity and chosen properties at every interval, plus start/end and min/max extents, in one call; it can also press keys at exact game times (inputs) and wait for another actor\'s property to change before it starts (startWhen).',
      whenToUse: [
        'A jump, spring launch, moving platform, enemy patrol or fall must be proven in PIE without a sleep-and-poll loop.',
        'The peak height, landing point or path of a moving actor is needed.',
        'An input must land at an exact moment (jump when a platform appears): inputs and startWhen run the whole timeline inside one call, so the caller\'s own delay between calls cannot shift it.',
      ],
      whenNotToUse: ['Only the current transform is needed (use get_transform).', 'Nothing is playing: the editor world does not simulate (start PIE with control_editor.play).'],
      inputProps: {
        actorName: P.pieActorName,
        durationSeconds: { type: 'number', description: 'Game seconds to watch (default 2, at most 30). Game time, so a clock slowed with set_game_speed still covers the same span of play; a run that needs more wall-clock time than maxRealSeconds stops there (endedBecause realTimeCap).' },
        intervalSeconds: { type: 'number', description: 'Game seconds between samples (default 0.05; 0 samples every frame). At most 400 samples are kept.' },
        propertyNames: { type: 'array', items: { type: 'string' }, description: 'Actor properties read at every sample, e.g. ["bDead", "HP"], or a component\'s as "Component.Property" ("Visual.RelativeScale3D" catches a squash on landing), the component by its name or by the actor property that holds it ("CharacterMovement.MaxWalkSpeed"); a name that resolves to nothing is listed under missingProperties.' },
        maxRealSeconds: { type: 'number', description: 'Wall-clock cap (default 25, at most 50): the run stops here and reports how much game time it covered, so a long run or an editor throttled to 3 fps still answers before a client that gives up at 30 s. Raise it only for a client that waits longer.' },
        inputs: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              key: { type: 'string', description: 'Key name as simulate_input takes it: SpaceBar, D, A, Left, Enter.' },
              atSeconds: { type: 'number', description: 'Game seconds after the run starts to press it (default 0).' },
              holdSeconds: { type: 'number', description: 'Game seconds to hold it (default 0.1).' },
            },
            required: ['key'],
            additionalProperties: false,
          },
          description: 'Keys pressed and released at exact game times during the run (at most 32), e.g. [{"key":"D","atSeconds":0,"holdSeconds":2},{"key":"SpaceBar","atSeconds":0.6,"holdSeconds":0.2}]. A key still held when the run ends is released.',
        },
        startWhen: {
          type: 'object',
          properties: {
            actorName: { type: 'string', description: 'Actor whose property starts the run.' },
            propertyName: { type: 'string', description: 'That actor\'s property, as propertyNames reads it: bActorEnableCollision, or a component\'s as Component.Property (CharacterMovement.MovementMode starts the run on a landing when equals is MOVE_Walking).' },
            equals: { description: 'Value that starts the run, as samples show it ("True", "False", 3).' },
            waitForChange: { type: 'boolean', description: 'Default true: start only when the value BECOMES equals (a platform appearing), not while it already is.' },
            maxWaitSeconds: { type: 'number', description: 'Game seconds to wait before giving up with endedBecause startWhenTimeout (default 10, at most 30).' },
          },
          required: ['actorName', 'propertyName', 'equals'],
          additionalProperties: false,
          description: 'Hold the run (samples and inputs) until another actor\'s property takes a value, so the timeline starts on a game event instead of whenever the call arrived.',
        },
      },
      required: ['actorName'],
      outputProps: {
        actorName: { type: 'string', description: 'The actor watched.' },
        samples: {
          type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true },
          description: 'One entry per sample: t (game seconds since the start), location [x, y, z], velocity [x, y, z], properties.',
        },
        sampleCount: { type: 'number', description: 'How many samples were taken.' },
        gameSeconds: { type: 'number', description: 'Game time covered.' },
        realSeconds: { type: 'number', description: 'Wall-clock time the run took.' },
        endedBecause: { type: 'string', description: 'duration, realTimeCap, sampleCap, actorDestroyed (a PIE death that reloads the level ends here), worldEnded (PIE stopped), startWhenTimeout (startWhen never happened within maxWaitSeconds) or gamePaused (game time stood still for 3 s: a title or pause menu holds the game; the warning says how to get past it).' },
        inputsApplied: {
          type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true },
          description: 'One entry per input: key, at, hold, and down/up, the game seconds since the start when it was pressed and released (up is when the run ended for a key still held).',
        },
        waitedSeconds: { type: 'number', description: 'Game seconds spent waiting for startWhen before the run began.' },
        start: { type: 'array', items: { type: 'number' }, description: 'First sampled location.' },
        end: { type: 'array', items: { type: 'number' }, description: 'Last sampled location.' },
        min: { type: 'array', items: { type: 'number' }, description: 'Smallest x, y and z sampled (the lowest point is min[2]).' },
        max: { type: 'array', items: { type: 'number' }, description: 'Largest x, y and z sampled (the peak height is max[2]).' },
        missingProperties: { type: 'array', items: { type: 'string' }, description: 'propertyNames the actor\'s class does not have.' },
        windowRestored: { type: 'boolean', description: 'True when the editor window was minimized: a minimized editor runs PIE at about 3 fps, so it was put back on screen, without taking focus, before the run and is minimized again when the run ends. The background CPU throttle (Use Less CPU when in Background) is off for the run only, whatever the window did.' },
      },
      outputRequired: [],
      effect: 'read',
      costLatency: 'interactive',
      exampleInput: { action: 'sample_motion', actorName: 'PlayerPawn', durationSeconds: 1.5, propertyNames: ['bDead'] },
      exampleOutput: {
        success: true, message: '31 samples of BP_Mario_C_0 over 1.50 game seconds (duration)',
        actorName: 'BP_Mario_C_0', sampleCount: 31, gameSeconds: 1.5, realSeconds: 3.2, endedBecause: 'duration',
        samples: [{ t: 0, location: [8500, 0, 56.1], velocity: [0, 0, 0], properties: { bDead: 'False' } }],
        start: [8500, 0, 56.1], end: [9310, 0, 130.2], min: [8500, 0, 56.1], max: [9310, 0, 302.4],
      },
    }),
  },
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'apply_force',
    topics: ['push actor', 'apply impulse'],
    domain: DOMAIN,
    family: FAMILY_PHYSICS,
    summary: 'Apply a physics force vector to an actor, auto-enabling simulation on failure.',
    whenToUse: ['A physical impulse must be applied to a simulated actor.'],
    whenNotToUse: ['The actor is not physics-enabled and physics cannot be auto-enabled.'],
    inputProps: { actorName: P.actorName, force: P.force },
    required: ['actorName', 'force'],
    effect: 'write',
    costLatency: 'interactive',
    exampleInput: { action: 'apply_force', actorName: 'Cube1', force: [0, 0, 500] },
  }),
];
