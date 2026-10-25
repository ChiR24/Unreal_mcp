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
    summary: 'Detach an actor from its parent, keeping where it stands in the world (KeepWorldTransform) and clearing its owner, in one undo step. An actor attached to nothing answers success with detached false. While Play In Editor runs it detaches the playing copy, not the editor actor.',
    whenToUse: ['An actor must stop following its parent (a picked-up item dropped, a part taken off a vehicle).'],
    whenNotToUse: ['The actor is attached to nothing: the call answers detached false.'],
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
    summary: 'Set Blueprint variables on one placed actor (actorName, the playing copy during Play) from variables {name: value}, or on many actors with actors, each with its own; an actor reference takes an actor\'s name. Every name is checked first: one the class lacks refuses the call (PROPERTY_NOT_FOUND, listing its variables) and nothing is set. The construction script reruns, as a Details panel edit does, and the call is one undo step (a batch too). A value a variable will not take fails with VARIABLE_NOT_SET naming it (VARIABLE_BATCH_INCOMPLETE for actors); updated lists what was set.',
    whenToUse: ['Instance variables on a Blueprint actor must be configured.', 'Several actors each need their own variable values (actors).'],
    whenNotToUse: ['The actor is not a Blueprint instance (variables are undefined).'],
    inputProps: {
      actorName: P.actorName,
      variables: {
        ...P.variables,
        description: 'Blueprint variable name to value map. A name the actor does not have refuses the whole call before anything is set; the error lists the variables it does have. An actor variable takes the other actor by label, name or path. A value that does not take (an object path that loads nothing) fails the call, naming it; the variables that did take are listed under updated.',
      },
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
    summary: 'Remember an actor\'s transform (location, rotation and scale only) under snapshotName, in editor memory, to put back later (inspect create_snapshot with snapshotOp restore). A name used again replaces the old snapshot; snapshots last until the editor closes.',
    whenToUse: ['An actor\'s placement must be kept before trying a move, to put it back exactly.'],
    whenNotToUse: ['Other state (properties, visibility, tags) must come back too: only the transform is kept.', 'A recorded change should be reverted: control_editor undo takes it back.'],
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
    summary: 'Turn collision on or off for an actor, give its components a collision preset (BlockAll, OverlapAllDynamic, NoCollision...), or set how they answer one channel (Pawn Ignore lets a character walk through, Pawn Block stops it); many actors at once with actorNames, as one undoable step.',
    topics: ['collision preset', 'collision response', 'walk through', 'block the player', 'ignore collision with player'],
    whenToUse: [
      'Actor collision must be toggled.',
      'Many actors must have collision toggled together (actorNames does them in one call).',
      'A character must walk through an actor or be stopped by it (responses {"Pawn": "Ignore"} or {"Pawn": "Block"}), or the actor must only report overlaps, as a trigger does (profile OverlapAllDynamic).',
    ],
    whenNotToUse: [
      'A mode other than on and off (QueryOnly, PhysicsOnly): use edit_component (edit set_properties) with CollisionEnabled, which takes the ECollisionEnabled names.',
      'Every placed copy of a class must change: use edit_scs set_property on its component with CollisionProfileName or CollisionResponses.',
    ],
    inputProps: {
      actorName: P.actorName,
      actorNames: { type: 'array', items: { type: 'string' }, description: 'Several actors to change in one call and one undo step, in place of actorName; names not found are listed back under missing, and the actors changed under affectedActors (which the receipt lists as changes, with an actor handle each).' },
      collisionEnabled: { ...P.collisionEnabled, description: 'Turn collision on (true) or off (false). Omitted with profile or responses, the mode stays as the preset sets it.' },
      profile: str('A collision preset of the project (BlockAll, BlockAllDynamic, OverlapAll, OverlapAllDynamic, NoCollision, Pawn, Trigger, PhysicsActor...); an unknown name is refused with the list. The reply reads the preset back.'),
      responses: {
        type: 'object',
        description: 'How the components answer each named channel, {"Pawn": "Ignore", "Visibility": "Block"}: an engine channel (WorldStatic, WorldDynamic, Pawn, Visibility, Camera, PhysicsBody, Vehicle, Destructible) or one the project added, by its name; each Block, Overlap or Ignore. Applied after profile, so the preset then reads Custom; the reply reads each channel back.',
        additionalProperties: true,
        'x-unreal-reflection-boundary': true,
      },
      componentName: str('Only the primitive component of this name (the capsule of a character, not its mesh); every primitive component of the actor otherwise.'),
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
