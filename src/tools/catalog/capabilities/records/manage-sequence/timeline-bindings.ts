/**
 * Timeline binding records: add_camera, add_actor, add_actors,
 * remove_actors, get_bindings, add_spawnable_from_class, add_keyframe.
 *
 * Grounded in sequence-core-actions.ts (add_camera/add_actor/add_actors/
 * remove_actors/get_bindings/add_keyframe/add_spawnable_from_class) and
 * the native HandleSequence*Bindings bodies.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildRecord, P, SEQ_PLUGINS } from './helpers.js';

const F = 'timeline';
const D = 'sequence';

export const TIMELINE_BINDINGS_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'sequence.add_camera', action: 'add_camera', family: F, domain: D,
    summary: 'Add a camera to a Level Sequence: by default a CameraActor placed at the world origin (actorName labels it, default SequenceCamera; move it with control_actor) and bound as a possessable; spawnable true adds a camera the sequence owns, which exists only while it plays. An asset that is not a Level Sequence is refused before anything is placed, and a camera that could not be bound is removed again (BIND_FAILED).',
    whenToUse: ['A camera must be added to the sequence for cinematic shots.'],
    whenNotToUse: ['The camera already exists as a binding.'],
    inputProps: { path: P.path, actorName: P.actorName, spawnable: { type: 'boolean', description: 'Add a spawnable camera owned by the sequence instead of placing a camera actor in the level (default false).' } },
    required: ['path'],
    outputProps: { bindingGuid: { type: 'string', description: 'The binding GUID.' }, actorLabel: { type: 'string', description: 'Label of the camera actor placed in the level (a placed camera only).' }, spawnable: { type: 'boolean', description: 'True for a camera the sequence owns.' } },
    outputRequired: [],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'add_camera', path: '/Game/Cinematics/SEQ_Master' },
  }),
  buildRecord({
    id: 'sequence.add_actor', action: 'add_actor', family: F, domain: D,
    summary: 'Bind a placed actor (actorName, its label) to a Level Sequence as a possessable, reusing the binding it already has; a missing actor fails with NO_ACTORS_BOUND. Answers results with the bindingGuid.',
    whenToUse: ['An existing actor must be added to the sequence.'],
    whenNotToUse: ['The actor does not exist in the current level.'],
    inputProps: { path: P.path, actorName: P.actorName },
    required: ['path', 'actorName'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'add_actor', path: '/Game/Cinematics/SEQ_Master', actorName: 'MyActor' },
  }),
  buildRecord({
    id: 'sequence.add_actors', action: 'add_actors', family: F, domain: D,
    summary: 'Bind several placed actors (actorNames, their labels) to a Level Sequence as possessables, reusing bindings they already have; answers each in results (bindingGuid or error) with successful and failed, and fails with NO_ACTORS_BOUND only when none bound.',
    whenToUse: ['Multiple actors must be bound at once.'],
    whenNotToUse: ['Only a single actor is needed.'],
    inputProps: { path: P.path, actorNames: P.actorNames },
    required: ['path', 'actorNames'],
    // Native HandleSequenceAddActors builds a per-actor results[] plus total/successful/failed
    // (SequenceHandlersBindings.cpp:198-216). Declaring none of it made output validation drop
    // the lot, so binding two nonexistent actors returned a bare "Actors processed" success.
    outputProps: { results: { type: 'array', items: { type: 'object', description: 'Per-actor binding result.', additionalProperties: true, 'x-unreal-reflection-boundary': true }, description: 'Per-actor result: actorName, success, bindingGuid or error.' }, total: { type: 'number', description: 'Actor names supplied.' }, successful: { type: 'number', description: 'Actors actually bound.' }, failed: { type: 'number', description: 'Actor names that could not be bound.' } },
    outputRequired: ['results', 'total', 'successful', 'failed'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'add_actors', path: '/Game/Cinematics/SEQ_Master', actorNames: ['Actor1', 'Actor2'] },
    exampleOutput: { success: true, message: 'Actors processed', total: 2, successful: 2, failed: 0, results: [{ actorName: 'Actor1', success: true, bindingGuid: 'ABC-123' }] },
  }),
  buildRecord({
    id: 'sequence.remove_actors', action: 'remove_actors', family: F, domain: D,
    summary: 'Remove the bindings of the named actors (actorNames) from a Level Sequence, with their tracks; answers removedActors and removed, and fails with NO_ACTORS_REMOVED when none of them was bound.',
    whenToUse: ['Bound actors must be removed from the sequence.'],
    whenNotToUse: ['The actors are not bound to the sequence.'],
    inputProps: { path: P.path, actorNames: P.actorNames },
    required: ['path', 'actorNames'],
    outputProps: { results: { type: 'array', items: { type: 'object', description: 'Per-actor removal result.', additionalProperties: true, 'x-unreal-reflection-boundary': true }, description: 'Per-actor result: actorName, success or error.' }, total: { type: 'number', description: 'Actor names supplied.' }, successful: { type: 'number', description: 'Bindings actually removed.' }, failed: { type: 'number', description: 'Actor names with no binding to remove.' } },
    outputRequired: ['results', 'total', 'successful', 'failed'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'remove_actors', path: '/Game/Cinematics/SEQ_Master', actorNames: ['Actor1'] },
    exampleOutput: { success: true, message: 'Actors processed', total: 1, successful: 1, failed: 0, results: [{ actorName: 'Actor1', success: true }] },
  }),
  buildRecord({
    id: 'sequence.get_bindings', action: 'get_bindings', family: F, domain: D,
    summary: 'List all bindings (possessables and spawnables) in a Level Sequence.',
    whenToUse: ['Sequence bindings must be enumerated.'],
    whenNotToUse: ['A specific binding GUID is already known.'],
    inputProps: { path: P.path },
    required: ['path'],
    // Native HandleSequenceGetBindings (SequenceHandlersSpawnables.cpp:89-147)
    // emits per binding {id, name} — id is the object-guid string at :117, the
    // former guid field is never emitted. Declared exactly.
    outputProps: { bindings: { type: 'array', items: { type: 'object', description: 'Binding info.', additionalProperties: false, properties: { id: { type: 'string', description: 'Binding GUID (object guid string).' }, name: P.actorName }, required: ['id', 'name'] }, description: 'Sequence bindings.' } },
    outputRequired: ['bindings'],
    effect: 'read', latency: 'instant', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'get_bindings', path: '/Game/Cinematics/SEQ_Master' },
    exampleOutput: { success: true, bindings: [{ id: 'ABC-123', name: 'Camera1' }] },
  }),
  buildRecord({
    id: 'sequence.add_spawnable_from_class', action: 'add_spawnable_from_class', family: F, domain: D,
    summary: 'Add a spawnable of className (a class path or name) to a Level Sequence: an actor the sequence creates while it plays and removes after. An unknown class is CLASS_NOT_FOUND; answers the className resolved and its bindingGuid.',
    whenToUse: ['A spawnable actor must be created from a class.'],
    whenNotToUse: ['An existing level actor should be possessed instead.'],
    inputProps: { path: P.path, className: P.className },
    required: ['path', 'className'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'add_spawnable_from_class', path: '/Game/Cinematics/SEQ_Master', className: '/Script/Engine.PointLight' },
  }),
  buildRecord({
    id: 'sequence.add_keyframe', action: 'add_keyframe', family: F, domain: D,
    summary: 'Add a keyframe for a property at a specific frame on a bound actor.',
    whenToUse: ['A property value must be animated at a specific frame.'],
    whenNotToUse: ['The actor is not bound to the sequence.'],
    inputProps: { path: P.path, actorName: P.actorName, bindingId: P.bindingId, property: { ...P.property, description: 'Property to key: Transform, Location, Rotation or Scale; Visibility, which shows (true) or hides (false) the bound actor in the editor and in renders; or a float or bool property by name.' }, frame: P.frame, value: { description: 'Keyframe value. For property "Transform" pass a composed object with any subset of {location:{x,y,z}, rotation:{pitch,yaw,roll}, scale:{x,y,z}}; each component supplied must carry all of its finite axes; a new Transform track starts from the current transform of the actor, so the parts a key leaves out keep their values. Add lookAt:{x,y,z} beside location to aim the key at that point (a camera at its subject): pitch and yaw are worked out, roll comes from rotation.roll or 0, and the yaw turns the short way from the key before, so an orbit of aimed keys never spins round. For "Location"/"Rotation"/"Scale" pass that component object alone. Visibility takes true or false; other properties take their own scalar value, so no type is declared here.' }, interpolation: { type: 'string', enum: ['auto', 'linear', 'constant'], description: 'How the curve leaves this key: auto (default) a smooth curve through the keys, linear a steady rate to the next key (a bullet, a constant pan), constant a hold that jumps at the next key.' }, keys: { type: 'array', minItems: 1, maxItems: 500, items: { type: 'object', properties: { frame: { type: 'integer', description: 'Frame number for this key.' }, value: { description: 'Key value, in the form value takes.' }, interpolation: { type: 'string', enum: ['auto', 'linear', 'constant'], description: 'How the curve leaves this key; default the interpolation of the call.' } }, required: ['frame', 'value'], additionalProperties: false }, description: 'Several keys on this property in one call, in place of frame and value: a whole curve, or a flash shown on one frame and hidden on the next. Each entry is {frame, value, interpolation}; they are written in order, so lookAt keys turn the short way from the entry before. Every frame is checked before any key is written.' } },
    required: ['path'],
    requiredOneOf: ['bindingId', 'actorName'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'add_keyframe', path: '/Game/Cinematics/SEQ_Master', actorName: 'Cube', property: 'Transform', frame: 0, value: { location: { x: 0, y: 0, z: 100 } } },
  }),
];
