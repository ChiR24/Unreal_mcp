import type { CapabilityRecordSource, JsonObject } from '../../model.js';
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
  rpcType: str('Server, Client or NetMulticast (Multicast is accepted too); anything else is refused.'),
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
  r('configure_net_update_frequency', 'Configure actor network update frequency.', ['blueprintPath', 'netUpdateFrequency', 'minNetUpdateFrequency'], ['blueprintPath']),
  r('configure_net_priority', 'Configure actor network bandwidth priority.', ['blueprintPath', 'netPriority'], ['blueprintPath']),
  r('set_net_dormancy', 'Set an Actor Blueprint\'s default network dormancy.', ['blueprintPath', 'dormancy'], ['blueprintPath', 'dormancy']),
  r('configure_replication_graph', 'Set an Actor Blueprint\'s spatially loaded (World Partition) and net load on client defaults; only the fields sent are written.', ['blueprintPath', 'spatiallyLoaded', 'netLoadOnClient'], ['blueprintPath']),
  r('create_rpc_function', 'Create a Blueprint RPC function (Server, Client or NetMulticast).', ['blueprintPath', 'functionName', 'rpcType', 'reliable'], ['blueprintPath', 'functionName', 'rpcType'], ['functionName'], ['functionName']),
  r('configure_rpc_validation', 'Configure RPC validation.', ['blueprintPath', 'functionName', 'withValidation'], ['blueprintPath', 'functionName']),
  r('set_rpc_reliability', 'Set RPC reliability.', ['blueprintPath', 'functionName', 'reliable'], ['blueprintPath', 'functionName', 'reliable']),
  r('set_owner', 'Set or clear (empty ownerActorName) the owner of a runtime actor; an owner name that matches no actor is refused.', ['actorName', 'ownerActorName'], ['actorName'], [], [], false, true),
  r('set_autonomous_proxy', 'Make every replicated variable of a Blueprint replicate to the autonomous proxy only (COND_AutonomousOnly), or undo that; fails when it has no replicated variable.', ['blueprintPath', 'isAutonomousProxy'], ['blueprintPath']),
  r('check_has_authority', 'Read runtime actor authority state.', ['actorName'], ['actorName'], ['hasAuthority', 'role'], ['hasAuthority'], true, true),
  r('check_is_locally_controlled', 'Read local-control state for an actor.', ['actorName'], ['actorName'], ['isLocallyControlled', 'isLocalController'], ['isLocallyControlled'], true, true),
  r('configure_net_cull_distance', 'Configure network relevancy cull distance.', ['blueprintPath', 'netCullDistanceSquared', 'useOwnerNetRelevancy'], ['blueprintPath']),
  r('set_always_relevant', 'Set always-relevant replication behavior.', ['blueprintPath', 'alwaysRelevant'], ['blueprintPath']),
  r('set_only_relevant_to_owner', 'Set owner-only relevancy behavior.', ['blueprintPath', 'onlyRelevantToOwner'], ['blueprintPath']),
  r('set_replicated_using', 'Assign a RepNotify function to a property.', ['blueprintPath', 'propertyName', 'repNotifyFunc'], ['blueprintPath', 'propertyName', 'repNotifyFunc']),
  r('configure_push_model', 'Turn push-model replication on or off for every replicated variable of a Blueprint; fails when it has none.', ['blueprintPath', 'usePushModel'], ['blueprintPath']),
  r('configure_client_prediction', 'Set whether a Character Blueprint always replicates its movement transform timestamp (CharacterMovement bNetworkAlwaysReplicateTransformUpdateTimestamp); other Blueprints are refused.', ['blueprintPath', 'enablePrediction'], ['blueprintPath', 'enablePrediction']),
  r('configure_server_correction', 'Set how long a Character Blueprint\'s simulated proxies smooth server corrections; other Blueprints are refused.', ['blueprintPath', 'smoothingRate'], ['blueprintPath', 'smoothingRate']),
  r('add_network_prediction_data', 'Add a Blueprint variable, or convert an existing one of the same type, that replicates only to the owning client (COND_AutonomousOnly) for client-side prediction state; the reply says whether the Blueprint itself replicates.', ['blueprintPath', 'dataType', 'variableName'], ['blueprintPath', 'dataType'], ['variableName', 'dataType', 'created', 'updated', 'actorReplicates', 'compiled', 'saved'], ['variableName', 'created', 'updated', 'actorReplicates', 'compiled', 'saved']),
  r('configure_movement_prediction', 'Set a Character Blueprint\'s movement network smoothing mode and smoothing distances; only the fields sent are written, other Blueprints are refused.', ['blueprintPath', 'networkSmoothingMode', 'networkMaxSmoothUpdateDistance', 'networkNoSmoothUpdateDistance'], ['blueprintPath']),
  r('configure_net_driver', 'Write the game net driver\'s rate limits to its class defaults and DefaultEngine.ini, and to the running net driver when there is one; only the fields sent are written.', ['maxClientRate', 'maxInternetClientRate', 'netServerMaxTickRate']),
  r('set_net_role', 'Choose how clients see an Actor Blueprint\'s actors: ROLE_None turns replication off, ROLE_SimulatedProxy or ROLE_AutonomousProxy turn it on.', ['blueprintPath', 'role'], ['blueprintPath', 'role']),
  r('configure_replicated_movement', 'Configure replicated movement.', ['blueprintPath', 'replicateMovement'], ['blueprintPath']),
  r('get_networking_info', 'Read networking state for a Blueprint or actor.', ['blueprintPath', 'actorName'], [], ['networkingInfo'], ['networkingInfo'], true),
];
