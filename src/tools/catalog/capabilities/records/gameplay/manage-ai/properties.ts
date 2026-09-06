/**
 * AI-local JSON-schema property fragments (behavior, perception, graph).
 *
 * Private to manage_ai. The shared gameplay ../properties.ts fragment map is
 * reused for names whose shape is identical across the gameplay domain; every
 * name below is either AI-only or differs in shape from the shared fragment
 * (notably vectors, which manage_ai carries in OBJECT form -- {x,y,z} /
 * {pitch,yaw,roll} -- not the shared 3-element array form).
 *
 * Grounded in the legacy manage_ai input schema recovered from
 * HEAD:src/tools/definitions/gameplay/ai/manage-ai-{behavior,runtime}-properties.ts,
 * the TS handler bodies (handlers/ai/ai-handlers.ts, ai-utility-actions.ts,
 * graph route via orchestration/consolidated-handler-registration.ts:205), and
 * the native AI domain under plugins/.../Private/Domains/AI/.
 *
 * Object-typed fragments are bounded (additionalProperties: false) because the
 * native handlers read a closed set of sub-keys; capabilities/json-schema.ts
 * rejects an unbounded object that carries no explicit reflection boundary.
 */
import type { JsonObject } from '../../../index.js';
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
  'PathingGrid', 'SimpleGrid', 'Composite', 'Custom',
] as const;

const EQS_TEST_TYPES = [
  'Distance', 'Dot', 'GameplayTags', 'Overlap', 'Pathfinding',
  'PathfindingBatch', 'Project', 'Random', 'Trace', 'Custom',
] as const;

export const AI = {
  // --- shared gameplay fragments reused verbatim (identical shape) ---
  action: P.action,
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
  keyName: str('Blackboard key name.'),
  keyType: pick('Blackboard key data type.',
    ['Bool', 'Int', 'Float', 'Vector', 'Rotator', 'Object', 'Class', 'Enum', 'Name', 'String']),
  baseObjectClass: str('Base class filter for Object/Class blackboard keys.'),
  isInstanceSynced: bool('Sync key across instances.'),

  // --- behavior tree nodes ---
  compositeType: pick('Composite node type.', ['Selector', 'Sequence', 'Parallel', 'SimpleParallel']),
  taskType: pick('Task node type.', TASK_TYPES),
  decoratorType: pick('Decorator node type.', DECORATOR_TYPES),
  serviceType: pick('Service node type.', ['DefaultFocus', 'RunEQS', 'Custom']),
  subnodeType: pick('Behavior Tree subnode kind for add_subnode.', ['Decorator', 'Service']),
  nodeClass: str('Node class path.'),
  nodeId: str('ID of the node.'),
  parentNodeId: str('ID of the parent node.'),
  childNodeId: str('ID of the child node.'),
  nodeType: str('Behavior Tree graph node type.'),
  comment: str('Comment text applied to the graph node.'),
  x: num('Graph node X coordinate.'),
  y: num('Graph node Y coordinate.'),

  // --- environment query system ---
  generatorType: pick('EQS generator type.', EQS_GENERATOR_TYPES),
  contextType: pick('EQS context type.',
    ['Querier', 'Item', 'EnvQueryContext_BlueprintBase', 'Custom']),
  testType: pick('EQS test type.', EQS_TEST_TYPES),
  testIndex: num('Index of the test to configure.'),
  generatorSettings: closed('Generator-specific settings.', {
    searchRadius: N, searchCenter: S, actorClass: S, gridSize: N,
    spacesBetween: N, innerRadius: N, outerRadius: N,
  }),
  testSettings: closed('Test scoring and filter settings.', {
    scoringEquation: { type: 'string', enum: ['Linear', 'Square', 'InverseLinear', 'Constant'] },
    clampMin: N, clampMax: N,
    filterType: { type: 'string', enum: ['Minimum', 'Maximum', 'Range'] },
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
    hearingRange: N, loSHearingRange: N, detectFriendly: B, maxAge: N,
  }),
  damageConfig: closed('AI damage sense configuration.', { maxAge: N }),
  teamId: num('Team ID for perception affiliation (0=Neutral, 1=Player, 2=Enemy, ...).'),
  dominantSense: pick('Dominant sense for perception prioritization.',
    ['Sight', 'Hearing', 'Damage', 'Touch', 'None']),
  enableSight: bool('Enable the sight sense.'),
  enableHearing: bool('Enable the hearing sense.'),
  enableDamage: bool('Enable the damage sense.'),
  sightRadius: num('Sight radius in world units.'),
  loseSightRadius: num('Radius at which sight is lost.'),
  peripheralVisionAngle: num('Peripheral vision half-angle in degrees.'),
  hearingRange: num('Hearing range in world units.'),
  focusActorName: str('Actor the controller should focus on.'),

  // --- state tree ---
  parentStateName: str('Parent state name for the added state.'),
  stateType: str('State kind for the added state.'),
  triggerType: str('Transition trigger kind.'),

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
