import type { CapabilityRecordSource, JsonObject } from '../../model.js';
import { buildCoreRecord } from '../core/builder.js';
import { utilityRecord, withAliases, withInputProps, withTopics } from '../utility/utility-record-builders.js';

const T = 'manage_networking' as const;
const EDIT = ['edit'] as const;
const RUNTIME = ['pie', 'simulate'] as const;
const bool = (description: string): JsonObject => ({ type: 'boolean', description });
const num = (description: string): JsonObject => ({ type: 'number', description });
const str = (description: string): JsonObject => ({ type: 'string', description });
// Types and descriptions the shared name-keyed pins get wrong for this family (a boolean published as a
// string, a content-free "Role."). The native handler refuses a value outside the listed names.
const NET_PROPS: Readonly<Record<string, JsonObject>> = {
  condition: str('Replication condition, a ELifetimeCondition name such as COND_OwnerOnly, COND_SkipOwner, COND_InitialOnly or COND_AutonomousOnly; an unknown name is refused.'),
  dormancy: str('Net dormancy, a ENetDormancy name: DORM_Never, DORM_Awake, DORM_DormantAll, DORM_DormantPartial or DORM_Initial; an unknown name is refused.'),
  role: str('How clients see the actor: ROLE_None (not replicated), ROLE_SimulatedProxy or ROLE_AutonomousProxy (replicated). ROLE_Authority is refused: the server always has it.'),
  rpcType: str('Where the call runs: Server (called on a client, on an actor its player owns, and run on the server), Client (called on the server, run on the owning player client) or NetMulticast (called on the server, run there and on every client). Multicast is accepted too, in any case; anything else is refused.'),
  functionName: str('The RPC. For setting=create, a new name that no function, variable, event or other member of the Blueprint or its parent class holds (NAME_TAKEN). For validation and reliability, an existing RPC: a function create made, or a custom event set to replicate in the editor (NOT_FOUND, or NOT_AN_RPC for one that does not replicate).'),
  reliable: bool('Reliable: resent until it arrives, in order. Unreliable calls can be dropped under load, which suits frequent cosmetic updates. Creating defaults to true.'),
  withValidation: bool('true is refused (NOT_SUPPORTED): only C++ RPCs have a validation step, so check the parameters at the start of the RPC body instead. false clears a validation flag an earlier call stored (changed says whether one was there).'),
  inputs: { type: 'array', description: 'RPC parameters, each {name, type}, with the types add_function inputs take (Float, Vector, Actor, Array<Int> ...), sent with every call. A nameless entry or a type no pin takes is refused (TYPE_RESOLUTION_FAILED) before anything changes. Default none.', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, 'x-unreal-reflection-boundary': true },
  repNotifyFunc: str('Function clients call when the variable\'s new value arrives; the variable also becomes replicated. It must take no parameters and return nothing: the Blueprint\'s own or an inherited one is used, a missing one is created empty (functionCreated), one with parameters or a return value is refused (the compiler drops it), and a name another member holds is NAME_TAKEN.'),
  spatiallyLoaded: bool('World Partition: stream the actor in by distance (AActor bIsSpatiallyLoaded).'),
  netLoadOnClient: bool('Load the placed actor on clients with the map (AActor bNetLoadOnClient).'),
  isAutonomousProxy: bool('true: every replicated variable replicates to the autonomous proxy only (COND_AutonomousOnly). false: variables using COND_AutonomousOnly go back to COND_None; other conditions stay.'),
  enablePrediction: bool('CharacterMovement bNetworkAlwaysReplicateTransformUpdateTimestamp: always replicate the transform timestamp simulated proxies smooth with.'),
  smoothingRate: num('Seconds simulated proxies take to smooth a server correction (location and rotation, dedicated and listen server).'),
  networkSmoothingMode: str('CharacterMovement NetworkSmoothingMode: Disabled, Linear, Exponential or Replay.'),
  networkMaxSmoothUpdateDistance: num('CharacterMovement NetworkMaxSmoothUpdateDistance in cm: corrections farther than this snap after smoothing.'),
  networkNoSmoothUpdateDistance: num('CharacterMovement NetworkNoSmoothUpdateDistance in cm: corrections farther than this snap without smoothing.'),
  maxClientRate: num('Game net driver MaxClientRate (bytes per second per LAN client).'),
  maxInternetClientRate: num('Game net driver MaxInternetClientRate (bytes per second per internet client).'),
  netServerMaxTickRate: num('Game net driver NetServerMaxTickRate (server ticks per second).'),
  dataType: str('Variable type in the add_variable grammar: Vector, Rotator, Transform, Float, Int, Bool, Name, Struct:/Game/Path/S_Name.S_Name, Array:Vector and so on.'),
  variableName: str('Variable to add or convert; default PredictionData_<dataType> (required when dataType is not a plain type name).'),
};
const r = (action: string, summary: string, params: readonly string[] = [], required: readonly string[] = [], outputs: readonly string[] = [], outputRequired: readonly string[] = [], read = false, runtime = false): CapabilityRecordSource => withInputProps(utilityRecord({
  tool: T, action, family: 'replication', summary, params, required, outputs, outputRequired,
  effect: read ? 'read' : 'write', states: runtime ? RUNTIME : EDIT,
  safeToRetry: read, dispatchAction: 'manage_networking',
}), Object.fromEntries(params.flatMap((name) => { const prop = NET_PROPS[name]; return prop === undefined ? [] : [[name, prop]]; })));

export const NETWORKING_REPLICATION_RECORDS: readonly CapabilityRecordSource[] = [
  withAliases(withTopics(r('set_property_replicated', 'Turn replication of a Blueprint variable on or off, optionally with its replication condition.', ['blueprintPath', 'propertyName', 'replicated', 'condition'], ['blueprintPath', 'propertyName']), ['replicate variable', 'replication', 'replicated property', 'network variable', 'enable replication', 'multiplayer variable', 'replicate', 'replicated variable', 'variable replication']), ['manage_networking.replicate_variable', 'manage_networking.enable_replication']),
  r('set_replication_condition', 'Set a Blueprint property replication condition.', ['blueprintPath', 'propertyName', 'condition'], ['blueprintPath', 'propertyName', 'condition']),
  r('configure_net_update_frequency', 'Configure actor network update frequency; only the fields sent are written.', ['blueprintPath', 'netUpdateFrequency', 'minNetUpdateFrequency'], ['blueprintPath']),
  r('configure_net_priority', 'Configure actor network bandwidth priority; refused without netPriority.', ['blueprintPath', 'netPriority'], ['blueprintPath']),
  r('set_net_dormancy', 'Set an Actor Blueprint\'s default network dormancy.', ['blueprintPath', 'dormancy'], ['blueprintPath', 'dormancy']),
  r('configure_replication_graph', 'Set an Actor Blueprint\'s spatially loaded (World Partition) and net load on client defaults; only the fields sent are written.', ['blueprintPath', 'spatiallyLoaded', 'netLoadOnClient'], ['blueprintPath']),
  r('create_rpc_function', 'Create a Blueprint RPC: a function graph named functionName whose calls run where rpcType says, with inputs as its parameters. Actor and ActorComponent Blueprints only.', ['blueprintPath', 'functionName', 'rpcType', 'reliable', 'inputs'], ['blueprintPath', 'functionName', 'rpcType'], ['functionName', 'rpcType', 'reliable', 'compiled', 'saved', 'actorReplicates'], ['functionName', 'rpcType', 'reliable', 'compiled', 'saved', 'actorReplicates']),
  r('configure_rpc_validation', 'Clear an RPC\'s validation flag (withValidation false); true is refused, since a Blueprint RPC has no validation step.', ['blueprintPath', 'functionName', 'withValidation'], ['blueprintPath', 'functionName'], ['functionName', 'withValidation', 'changed', 'compiled', 'saved'], ['functionName', 'withValidation', 'changed']),
  r('set_rpc_reliability', 'Make an RPC reliable or unreliable.', ['blueprintPath', 'functionName', 'reliable'], ['blueprintPath', 'functionName', 'reliable'], ['functionName', 'reliable', 'changed', 'compiled', 'saved'], ['functionName', 'reliable', 'changed']),
  r('set_owner', 'Set or clear (empty ownerActorName) the owner of a runtime actor; an owner name that matches no actor is refused, and so is an owner the actor already owns or the actor itself (OWNER_LOOP).', ['actorName', 'ownerActorName'], ['actorName'], [], [], false, true),
  r('set_autonomous_proxy', 'Make every replicated variable of a Blueprint replicate to the autonomous proxy only (COND_AutonomousOnly), or undo that; fails when it has no replicated variable.', ['blueprintPath', 'isAutonomousProxy'], ['blueprintPath']),
  r('check_has_authority', 'Read runtime actor authority state.', ['actorName'], ['actorName'], ['hasAuthority', 'role'], ['hasAuthority'], true, true),
  r('check_is_locally_controlled', 'Read local-control state for an actor.', ['actorName'], ['actorName'], ['isLocallyControlled', 'isLocalController'], ['isLocallyControlled'], true, true),
  r('configure_net_cull_distance', 'Configure network relevancy cull distance and owner relevancy; only the fields sent are written.', ['blueprintPath', 'netCullDistanceSquared', 'useOwnerNetRelevancy'], ['blueprintPath']),
  r('set_always_relevant', 'Set always-relevant replication behavior.', ['blueprintPath', 'alwaysRelevant'], ['blueprintPath']),
  r('set_only_relevant_to_owner', 'Set owner-only relevancy behavior.', ['blueprintPath', 'onlyRelevantToOwner'], ['blueprintPath']),
  r('set_replicated_using', 'Make a Blueprint variable RepNotify: it replicates, and clients call repNotifyFunc when its new value arrives.', ['blueprintPath', 'propertyName', 'repNotifyFunc'], ['blueprintPath', 'propertyName', 'repNotifyFunc'], ['repNotifyFunc', 'functionCreated', 'compiled', 'saved'], ['repNotifyFunc', 'functionCreated', 'compiled', 'saved']),
  r('configure_push_model', 'Clear a stale PushModel entry from every replicated variable of a Blueprint (usePushModel false; fails when it has none). true is refused: push model is set for the whole project by Net.IsPushModelEnabled and Net.MakeBpPropertiesPushModel, not per variable.', ['blueprintPath', 'usePushModel'], ['blueprintPath']),
  r('configure_client_prediction', 'Set whether a Character Blueprint always sends clients the server time stamp of its last movement update (CharacterMovement bNetworkAlwaysReplicateTransformUpdateTimestamp), which the linear smoothing of simulated proxies uses; it changes nothing unless the character replicates its movement. Other Blueprints are refused; saved (SAVE_FAILED when it cannot be written).', ['blueprintPath', 'enablePrediction'], ['blueprintPath', 'enablePrediction']),
  r('configure_server_correction', 'Set how long, in seconds, a Character Blueprint smooths the moves other machines see (smoothingRate: CharacterMovement NetworkSimulatedSmoothLocationTime and RotationTime and the listen-server pair, all four together). Other Blueprints are refused; saved (SAVE_FAILED when it cannot be written).', ['blueprintPath', 'smoothingRate'], ['blueprintPath', 'smoothingRate']),
  r('add_network_prediction_data', 'Add a Blueprint variable, or convert an existing one of the same type, that replicates only to the owning client (COND_AutonomousOnly) for client-side prediction state. Pawn, Character, Controller and ActorComponent Blueprints only (no other actor is an autonomous proxy); the reply says whether the Blueprint itself replicates.', ['blueprintPath', 'dataType', 'variableName'], ['blueprintPath', 'dataType'], ['variableName', 'dataType', 'created', 'updated', 'actorReplicates', 'compiled', 'saved'], ['variableName', 'created', 'updated', 'actorReplicates', 'compiled', 'saved']),
  r('configure_movement_prediction', 'Set a Character Blueprint\'s movement network smoothing mode and smoothing distances; only the fields sent are written, other Blueprints are refused; saved (SAVE_FAILED when it cannot be written).', ['blueprintPath', 'networkSmoothingMode', 'networkMaxSmoothUpdateDistance', 'networkNoSmoothUpdateDistance'], ['blueprintPath']),
  r('configure_net_driver', 'Write the game net driver\'s rate limits to its class defaults and DefaultEngine.ini, and to the running net driver when there is one; only the fields sent are written.', ['maxClientRate', 'maxInternetClientRate', 'netServerMaxTickRate']),
  r('set_net_role', 'Choose how clients see an Actor Blueprint\'s actors: ROLE_None turns replication off, ROLE_SimulatedProxy or ROLE_AutonomousProxy turn it on.', ['blueprintPath', 'role'], ['blueprintPath', 'role']),
  r('configure_replicated_movement', 'Configure replicated movement.', ['blueprintPath', 'replicateMovement'], ['blueprintPath']),
  withTopics(r('get_networking_info', 'Read an actor\'s or Blueprint\'s replication settings: replicates, always relevant, only relevant to owner, net update frequency, cull distance, priority and dormancy; for a live actor also its net role, remote role and authority.', ['blueprintPath', 'actorName'], [], ['networkingInfo'], ['networkingInfo'], true),
    ['replication settings', 'is actor replicated', 'net dormancy value']),
  buildCoreRecord({
    parentTool: T, action: 'audit_replication', dispatchAction: 'manage_networking', domain: 'networking', family: 'audit',
    topics: ['audit replication', 'multiplayer mistakes', 'rpc does not run on server', 'reliable rpc on tick', 'check a blueprint for network bugs'],
    summary: 'Read a Blueprint for the replication mistakes that only show with a second player, each with the node to fix: rpc_not_replicated (an RPC on an actor that does not replicate, so a Server or Multicast call runs only where it is made), reliable_on_tick (Event Tick reaches a reliable RPC every frame, which fills the reliable buffer and drops the client), polled_on_tick (Event Tick reads a replicated variable that has no RepNotify, polling for a change a RepNotify reports once). Nothing compiles or changes; a call to an RPC is judged by its compiled flags, so compile after changing one.',
    whenToUse: ['A Blueprint will run in multiplayer and should be checked before a two-player test.'],
    whenNotToUse: ['Its replication settings are wanted (get_networking_info).', 'Mistakes that show in single-player Play (manage_blueprint lint_blueprint).'],
    inputProps: {
      blueprintPath: { type: 'string', description: 'The Actor or ActorComponent Blueprint to read.' },
      maxFindings: { type: 'number', minimum: 1, maximum: 200, description: 'Most findings listed (1-200, default 50); counts and findingCount still count them all.' },
    },
    required: ['blueprintPath'],
    outputProps: {
      findings: { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, description: 'One row per finding: rule, graph, node (its title), nodeId (as edit_graph takes it) and detail (what is wrong and the call that fixes it).' },
      counts: { type: 'object', additionalProperties: { type: 'number' }, description: 'Findings per rule, listed or not.' },
      findingCount: { type: 'number', description: 'Every finding, listed or not.' },
      replicates: { type: 'boolean', description: 'Whether the Blueprint\'s actor or component replicates.' },
      rpcCount: { type: 'number', description: 'The RPCs (custom events and functions set to replicate) in its graphs.' },
      blueprintPath: { type: 'string', description: 'The Blueprint read.' },
    },
    outputRequired: [],
    effect: 'read',
    costLatency: 'interactive',
    exampleInput: { action: 'audit_replication', blueprintPath: '/Game/Blueprints/BP_Door' },
  }),
];
