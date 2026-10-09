/**
 * Attachment and advanced records: attach/attach_actor, detach/detach_actor,
 * set_blueprint_variables, create_snapshot, set_actor_collision,
 * call_actor_function.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { ANY_EDITOR_STATE, buildCoreRecord } from '../core/builder.js';
import { DOMAIN, P } from './properties.js';
import { bool, str, vec3 } from '../shared/schema-props.js';

const FAMILY_ATTACH = 'attachment';
const FAMILY_BLUEPRINT = 'blueprint';
const FAMILY_SNAPSHOT = 'snapshot';
const FAMILY_COLLISION = 'collision';
const FAMILY_FUNCTION = 'function';

export const ADVANCED_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'attach',
    domain: DOMAIN,
    family: FAMILY_ATTACH,
    topics: ['attach actor', 'parent actor', 'attach to actor', 'child actor', 'attach to socket', 'set actor parent'],
    summary: 'Attach a child actor to a parent actor in the scene hierarchy, or to a bone or socket of one of its components (a weapon or a case to a hand).',
    whenToUse: ['An actor must follow another actor transform.', 'A prop must ride a character\'s hand, head or other bone: socketName with snapToTarget, then tune the grip with relativeLocation/relativeRotation.'],
    whenNotToUse: ['The actor should remain independent (use detach).'],
    inputProps: {
      childActor: P.childActor, parentActor: P.parentActor,
      componentName: str('Component of the parent actor to attach to (default its root): on a character built from several meshes, the mesh that owns the bone.'),
      socketName: str('Bone or socket on that component to attach to (hand_r, weapon_r, head). Refused with SOCKET_NOT_FOUND when the component has neither.'),
      snapToTarget: bool('Snap the child onto the bone or socket instead of keeping its current world placement (default false).'),
      relativeLocation: vec3('With snapToTarget: offset from the bone or socket in its own space, as [x, y, z].'),
      relativeRotation: vec3('With snapToTarget: rotation relative to the bone or socket, as [pitch, yaw, roll] in degrees.'),
    },
    required: ['childActor', 'parentActor'],
    effect: 'write',
    costLatency: 'interactive',
    exampleInput: { action: 'attach', childActor: 'Sword', parentActor: 'Knight' },
  }),
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'detach',
    domain: DOMAIN,
    family: FAMILY_ATTACH,
    summary: 'Detach a child actor from its current parent.',
    whenToUse: ['An actor must be released from attachment.'],
    whenNotToUse: ['The actor is not attached (no-op).'],
    inputProps: { actorName: P.actorName },
    required: ['actorName'],
    effect: 'write',
    behavior: { idempotency: 'idempotent' },
    costLatency: 'interactive',
    exampleInput: { action: 'detach', actorName: 'Sword' },
  }),
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'set_blueprint_variables',
    topics: ['actor variable'],
    domain: DOMAIN,
    family: FAMILY_BLUEPRINT,
    summary: 'Set one or more Blueprint instance variables on a spawned actor, or on many actors in one call with actors.',
    whenToUse: ['Instance variables on a Blueprint actor must be configured.', 'Several actors each need their own variable values (actors).'],
    whenNotToUse: ['The actor is not a Blueprint instance (variables are undefined).'],
    inputProps: {
      actorName: P.actorName, variables: P.variables,
      actors: {
        type: 'array',
        items: { type: 'object', properties: { actorName: P.actorName, variables: P.variables }, required: ['actorName', 'variables'], additionalProperties: false },
        description: 'Many actors in one call, each {actorName, variables} with its own values; every actor is reported, the actors that took a variable come back under affectedActors (which the receipt lists as changes, with an actor handle each), and the call fails naming any that did not take all of its variables.',
      },
    },
    required: [],
    requiredOneOf: ['actorName', 'actors'],
    effect: 'write',
    behavior: { idempotency: 'idempotent' },
    costLatency: 'interactive',
    exampleInput: { action: 'set_blueprint_variables', actorName: 'Lamp1', variables: { Brightness: 2.0 } },
  }),
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'create_snapshot',
    domain: DOMAIN,
    family: FAMILY_SNAPSHOT,
    summary: 'Capture a named snapshot of actor state for later comparison.',
    whenToUse: ['Actor state must be recorded before a mutation.'],
    whenNotToUse: ['No later comparison or restore is planned.'],
    inputProps: { actorName: P.actorName, snapshotName: P.snapshotName },
    required: ['actorName', 'snapshotName'],
    effect: 'read',
    exampleInput: { action: 'create_snapshot', actorName: 'Cube1', snapshotName: 'before_move' },
  }),
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'set_actor_collision',
    domain: DOMAIN,
    family: FAMILY_COLLISION,
    summary: 'Enable or disable collision on an actor, or on many actors at once with actorNames, as one undoable step; normalizes to set_collision.',
    whenToUse: ['Actor collision must be toggled.', 'Many actors must have collision toggled together (actorNames does them in one call).'],
    whenNotToUse: ['One component only, or a mode other than on and off (QueryOnly, PhysicsOnly): use edit_component (edit set_properties) with CollisionEnabled, which takes the ECollisionEnabled names.'],
    inputProps: {
      actorName: P.actorName,
      actorNames: { type: 'array', items: { type: 'string' }, description: 'Several actors to toggle in one call and one undo step, in place of actorName; names not found are listed back under missing, and the actors changed under affectedActors (which the receipt lists as changes, with an actor handle each).' },
      collisionEnabled: P.collisionEnabled,
    },
    required: [],
    requiredOneOf: ['actorName', 'actorNames'],
    effect: 'write',
    behavior: { idempotency: 'idempotent' },
    exampleInput: { action: 'set_actor_collision', actorName: 'Cube1', collisionEnabled: true },
  }),
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'call_actor_function',
    editorStates: ANY_EDITOR_STATE,
    domain: DOMAIN,
    family: FAMILY_FUNCTION,
    summary:
      'Call a named function on an actor, or while Play In Editor runs on the game\'s GameInstance, GameMode or another framework object, with optional arguments; normalizes to call_function.',
    whenToUse: [
      'A Blueprint or native function on an actor must be invoked.',
      'A function of the running game\'s GameInstance (save, score, reset) must be called during Play In Editor: actorName GameInstance.',
      'A function of one of the actor\'s components must be called (a body\'s GetCenterOfMass, a movement component\'s StopMovementImmediately): componentName.',
    ],
    whenNotToUse: ['The function has side effects that cannot be undone.'],
    inputProps: {
      actorName: {
        ...P.actorName,
        description:
          'Target actor name in the current level; while Play In Editor runs, GameInstance, GameMode, GameState, PlayerController, PlayerPawn, PlayerState or HUD names that object of the running game (the GameInstance is no actor).',
      },
      componentName: str('Call the function on this component of the actor instead of on the actor itself (refused with COMPONENT_NOT_FOUND when the actor has none of that name).'),
      functionName: P.functionName,
      arguments: P.arguments,
    },
    required: ['actorName', 'functionName'],
    outputProps: { value: P.value },
    outputRequired: [],
    effect: 'destructive',
    policyOverride: { consent: 'elevated' },
    costLatency: 'interactive',
    exampleInput: { action: 'call_actor_function', actorName: 'Lamp1', functionName: 'ToggleLight' },
    exampleOutput: { success: true, message: 'Called ToggleLight on Lamp1', value: true },
  }),
];
