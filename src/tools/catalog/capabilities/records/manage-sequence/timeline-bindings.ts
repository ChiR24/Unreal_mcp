/**
 * Timeline binding records: add_camera, add_actor, add_actors,
 * remove_actors, get_bindings, add_spawnable_from_class, add_keyframe.
 *
 * Grounded in sequence-core-actions.ts (add_camera/add_actor/add_actors/
 * remove_actors/get_bindings/add_keyframe/add_spawnable_from_class) and
 * the native HandleSequence*Bindings bodies.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildRecord, P, SEQ_PLUGINS } from './helpers.js';

const F = 'timeline';
const D = 'sequence';
const NR = 'Distinct Sequencer binding operation with unique target actor and binding lifecycle.';

export const TIMELINE_BINDINGS_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'sequence.add_camera', action: 'add_camera', family: F, domain: D,
    summary: 'Add a camera binding (possessable or spawnable) to a Level Sequence.',
    whenToUse: ['A camera must be added to the sequence for cinematic shots.'],
    whenNotToUse: ['The camera already exists as a binding.'],
    inputProps: { action: P.action, path: P.path, actorName: P.actorName, spawnable: { type: 'boolean', description: 'Whether to add as spawnable.' } },
    required: ['action', 'path'],
    outputProps: { bindingGuid: { type: 'string', description: 'The binding GUID.' } },
    outputRequired: [],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'add_camera', path: '/Game/Cinematics/SEQ_Master' },
    exampleOutput: { success: true, message: 'Camera added' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildRecord({
    id: 'sequence.add_actor', action: 'add_actor', family: F, domain: D,
    summary: 'Bind an existing level actor to a Level Sequence as a possessable.',
    whenToUse: ['An existing actor must be added to the sequence.'],
    whenNotToUse: ['The actor does not exist in the current level.'],
    inputProps: { action: P.action, path: P.path, actorName: P.actorName },
    required: ['action', 'path', 'actorName'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'add_actor', path: '/Game/Cinematics/SEQ_Master', actorName: 'MyActor' },
    exampleOutput: { success: true, message: 'Actor added' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildRecord({
    id: 'sequence.add_actors', action: 'add_actors', family: F, domain: D,
    summary: 'Bind multiple existing level actors to a Level Sequence.',
    whenToUse: ['Multiple actors must be bound at once.'],
    whenNotToUse: ['Only a single actor is needed.'],
    inputProps: { action: P.action, path: P.path, actorNames: P.actorNames },
    required: ['action', 'path', 'actorNames'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'add_actors', path: '/Game/Cinematics/SEQ_Master', actorNames: ['Actor1', 'Actor2'] },
    exampleOutput: { success: true, message: 'Actors added' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildRecord({
    id: 'sequence.remove_actors', action: 'remove_actors', family: F, domain: D,
    summary: 'Remove actor bindings from a Level Sequence.',
    whenToUse: ['Bound actors must be removed from the sequence.'],
    whenNotToUse: ['The actors are not bound to the sequence.'],
    inputProps: { action: P.action, path: P.path, actorNames: P.actorNames },
    required: ['action', 'path', 'actorNames'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'remove_actors', path: '/Game/Cinematics/SEQ_Master', actorNames: ['Actor1'] },
    exampleOutput: { success: true, message: 'Actors removed' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildRecord({
    id: 'sequence.get_bindings', action: 'get_bindings', family: F, domain: D,
    summary: 'List all bindings (possessables and spawnables) in a Level Sequence.',
    whenToUse: ['Sequence bindings must be enumerated.'],
    whenNotToUse: ['A specific binding GUID is already known.'],
    inputProps: { action: P.action, path: P.path },
    required: ['action', 'path'],
    outputProps: { bindings: { type: 'array', items: { type: 'object', description: 'Binding info.', additionalProperties: false, properties: { name: P.actorName, guid: { type: 'string', description: 'Binding GUID.' } }, required: ['name'] }, description: 'Sequence bindings.' } },
    outputRequired: ['bindings'],
    effect: 'read', latency: 'instant', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'get_bindings', path: '/Game/Cinematics/SEQ_Master' },
    exampleOutput: { success: true, bindings: [{ name: 'Camera1', guid: 'ABC-123' }] },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildRecord({
    id: 'sequence.add_spawnable_from_class', action: 'add_spawnable_from_class', family: F, domain: D,
    summary: 'Add a spawnable binding from a Unreal class path to a Level Sequence.',
    whenToUse: ['A spawnable actor must be created from a class.'],
    whenNotToUse: ['An existing level actor should be possessed instead.'],
    inputProps: { action: P.action, path: P.path, className: P.className },
    required: ['action', 'path', 'className'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'add_spawnable_from_class', path: '/Game/Cinematics/SEQ_Master', className: '/Script/Engine.PointLight' },
    exampleOutput: { success: true, message: 'Spawnable added' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildRecord({
    id: 'sequence.add_keyframe', action: 'add_keyframe', family: F, domain: D,
    summary: 'Add a keyframe for a property at a specific frame on a bound actor.',
    whenToUse: ['A property value must be animated at a specific frame.'],
    whenNotToUse: ['The actor is not bound to the sequence.'],
    inputProps: { action: P.action, path: P.path, actorName: P.actorName, bindingId: P.bindingId, property: P.property, frame: P.frame, value: P.value },
    required: ['action', 'path', 'frame'],
    requiredOneOf: ['bindingId', 'actorName'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'add_keyframe', path: '/Game/Cinematics/SEQ_Master', actorName: 'Cube', property: 'Transform', frame: 0, value: { location: { x: 0, y: 0, z: 100 } } },
    exampleOutput: { success: true, message: 'Keyframe added' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
];
