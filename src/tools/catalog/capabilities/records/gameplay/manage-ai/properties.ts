/**
 * AI-local JSON-schema property fragments (behavior, perception, graph).
 */
import type { JsonObject } from '../../../model.js';
import { P } from '../properties.js';
import { str, num, bool } from '../../shared/schema-props.js';

const pick = (description: string, values: readonly string[]): JsonObject =>
  ({ type: 'string', enum: [...values], description });
const closed = (description: string, properties: JsonObject): JsonObject =>
  ({ type: 'object', properties, description, additionalProperties: false });

const N: JsonObject = { type: 'number' };
const B: JsonObject = { type: 'boolean' };
const S: JsonObject = { type: 'string' };

/** Vector carried as an object with x/y/z, per the native AI accessors. */
export const xyz = (description: string): JsonObject => closed(description, { x: N, y: N, z: N });

const TASK_TYPES = [
  'MoveTo', 'MoveDirectlyToward', 'RotateToFaceBBEntry', 'Wait', 'WaitBlackboardTime',
  'PlayAnimation', 'PlaySound', 'RunEQSQuery', 'RunBehaviorDynamic', 'SetBlackboardValue',
  'PushPawnAction', 'FinishWithResult', 'MakeNoise', 'GameplayTaskBase', 'Custom',
] as const;

const DECORATOR_TYPES = [
  'Blackboard', 'BlackboardBased', 'CompareBBEntries', 'Cooldown', 'ConeCheck',
  'DoesPathExist', 'IsAtLocation', 'IsBBEntryOfClass', 'KeepInCone', 'Loop',
  'SetTagCooldown', 'TagCooldown', 'TimeLimit', 'ForceSuccess', 'ConditionalLoop', 'Custom',
] as const;

const EQS_GENERATOR_TYPES = [
  'ActorsOfClass', 'CurrentLocation', 'Donut', 'OnCircle',
  'PathingGrid', 'SimpleGrid', 'Composite',
] as const;

const EQS_TEST_TYPES = [
  'Distance', 'Dot', 'GameplayTags', 'Overlap', 'Pathfinding',
  'PathfindingBatch', 'Project', 'Random', 'Trace',
] as const;

export const AI = {
  // --- shared gameplay fragments reused verbatim (identical shape) ---
  name: P.name,
  path: P.path,
  assetPath: P.assetPath,
  blueprintPath: P.blueprintPath,
  actorName: P.actorName,
  controllerPath: P.controllerPath,
  behaviorTreePath: P.behaviorTreePath,
  blackboardPath: P.blackboardPath,
  stateTreePath: P.stateTreePath,
  stateName: P.stateName,
  fromState: P.fromState,
  toState: P.toState,
  properties: P.properties,
  value: P.value,
  save: P.save,

  // --- asset paths owned by manage_ai ---
  savePath: str('Directory path used when saving the created Behavior Tree.'),
  queryPath: str('Canonical /Game Environment Query asset path.'),
  definitionPath: str('Canonical /Game Smart Object definition asset path.'),
  configPath: str('Canonical /Game Mass Entity config asset path.'),
  componentName: str('Name of the component to add.'),

  // --- blackboard ---
  keyName: str('Blackboard key name, matched in any case.'),
  keyType: pick('Blackboard key data type.',
    ['Bool', 'Int', 'Float', 'Vector', 'Rotator', 'Object', 'Class', 'Enum', 'Name', 'String']),
  baseObjectClass: str('Class an Object key holds or a Class key names, as a class path or name (e.g. /Script/Engine.Actor); Object and Class keys only. Without it the key is based on Object, which actor-only key filters such as the MoveTo target skip.'),
  enumType: str('Enum an Enum key holds, by path (/Game/Enums/E_State, /Script/Engine.ECollisionChannel); Enum keys only. Without it the key has no enumerators to hold.'),
  isInstanceSynced: bool('Share the value of the key across every Blackboard component using this asset, so a value one AI sets reaches them all.'),

  // --- behavior tree nodes ---
  compositeType: pick('Composite node type: Selector, Sequence, or SimpleParallel (Parallel is the same).', ['Selector', 'Sequence', 'Parallel', 'SimpleParallel']),
  taskType: pick('Task node type.', TASK_TYPES),
  decoratorType: pick('Decorator node type.', DECORATOR_TYPES),
  serviceType: pick('Service node type.', ['DefaultFocus', 'RunEQS', 'Custom']),
  subnodeType: pick('Behavior Tree subnode kind for add_subnode.', ['Decorator', 'Service']),
  nodeClass: str('Node class path.'),
  nodeId: str('ID of the node.'),
  parentNodeId: str("ID of the parent node: 'root', a node GUID, or a node id as returned by add_composite/add_task (for example BTComposite_Selector_0)."),
  childNodeId: str('ID of the child node.'),
  nodeType: str('Behavior Tree graph node type.'),
  comment: str('Comment text applied to the graph node.'),
  x: num('Graph node X coordinate.'),
  y: num('Graph node Y coordinate.'),

  // --- environment query system ---
  generatorType: pick('EQS generator type.', EQS_GENERATOR_TYPES),
  contextType: pick('Querier (the AI running the query), Item (each item being tested), or Custom with contextClass.',
    ['Querier', 'Item', 'Custom']),
  contextClass: str('Context class for contextType Custom: a Blueprint based on EnvQueryContext_BlueprintBase, or a context class path.'),
  contextProperty: str('Context property of the generator or test to set (SearchCenter, GenerateAround, DistanceTo, Context); default the first one it has.'),
  optionIndex: num('Option whose generator takes the context when no testIndex is sent (default 0).'),
  testType: pick('EQS test type.', EQS_TEST_TYPES),
  testIndex: num('Test index counted across all options of the query, in the order the tests were added (the testIndex add_eqs_test returned).'),
  generatorSettings: closed('Settings the generator reads, refused when it does not: ActorsOfClass searchRadius (also keeps items inside it) and actorClass; SimpleGrid and PathingGrid gridSize (half the grid side) and spacesBetween; OnCircle searchRadius or outerRadius (the circle radius) and spacesBetween; Donut innerRadius and outerRadius. Where a generator centers is set with add_eqs_context.', {
    searchRadius: N, actorClass: S, gridSize: N,
    spacesBetween: N, innerRadius: N, outerRadius: N,
  }),
  testSettings: closed('Test scoring and filter settings.', {
    scoringEquation: { type: 'string', enum: ['Linear', 'Square', 'InverseLinear', 'SquareRoot', 'Constant'] },
    clampMin: N, clampMax: N,
    filterType: { type: 'string', enum: ['Minimum', 'Maximum', 'Range', 'Match'] },
    floatMin: N, floatMax: N,
  }),

  // --- perception ---
  sightConfig: closed('AI sight sense configuration.', {
    sightRadius: N, loseSightRadius: N, peripheralVisionAngle: N,
    pointOfViewBackwardOffset: N, nearClippingRadius: N, autoSuccessRange: N, maxAge: N,
    detectionByAffiliation: closed('Affiliation detection flags.',
      { enemies: B, neutrals: B, friendlies: B }),
  }),
  hearingConfig: closed('AI hearing sense configuration.', {
    hearingRange: N, detectFriendly: B, maxAge: N,
  }),
  damageConfig: closed('AI damage sense configuration.', { maxAge: N }),
  teamId: num('Team id to store, 0 to 254 (255 means no team).'),
  dominantSense: pick('Dominant sense for perception prioritization.',
    ['Sight', 'Hearing', 'Damage', 'Touch', 'None']),
  enableSight: bool('Enable the sight sense.'),
  enableHearing: bool('Enable the hearing sense.'),
  enableDamage: bool('Enable the damage sense.'),
  sightRadius: num('Sight radius in world units.'),
  loseSightRadius: num('Radius at which sight is lost.'),
  peripheralVisionAngle: num('Peripheral vision half-angle in degrees.'),
  hearingRange: num('Hearing range in world units.'),
  focusPlayer: bool('Focus the pawn of player 0, picked up again when it spawns late or respawns.'),
  focusTag: str('Focus the first actor carrying this tag (mark the target with control_actor add_tag).'),

  // --- state tree ---
  parentStateName: str('Parent state name for the added state.'),
  stateType: str('State (default), Group, Linked, LinkedAsset (UE 5.4+) or Subtree.'),
  triggerType: str('OnStateCompleted (default), OnStateSucceeded, OnStateFailed, OnTick, OnEvent (needs an event tag) or OnDelegate (newer engines).'),
  schema: str('What runs the tree: component (default, a StateTreeComponent on any actor), ai (a StateTreeAIComponent on an AI controller, UE 5.4+), or the class path of another State Tree schema (a UStateTreeSchema subclass, such as /Script/MassAIBehavior.MassStateTreeSchema for Mass entities).'),

  // --- smart objects / mass ---
  slotIndex: num('Index of the slot to configure.'),
  enabled: bool('Whether the slot is enabled.'),
  offset: xyz('Slot offset relative to the definition origin.'),
  rotation: closed('Rotation as {pitch, yaw, roll} in degrees.',
    { pitch: N, yaw: N, roll: N }),
    spawnCount: num('Number of entities the spawner creates.'),

    // --- promoted native routes ---
    failsafeToDefaultNavmesh: bool('Fall back to the default navmesh area when the modifier area class is unset.'),
    maxWalkSpeed: num('Maximum ground speed; left unchanged when omitted.'),
    maxAcceleration: num('Maximum acceleration; left unchanged when omitted.'),
    brakingDeceleration: num('Braking deceleration while walking; left unchanged when omitted.'),
    rotationRate: num('Yaw rotation rate in degrees per second; left unchanged when omitted.'),
  } satisfies Record<string, JsonObject>;
