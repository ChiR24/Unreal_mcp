import type { CapabilityRecordSource } from '../../index.js';
import { utilityRecord } from '../utility/helpers.js';

const T = 'manage_networking' as const;
const EDIT = ['edit'] as const;
const RUNTIME = ['pie', 'simulate'] as const;
const r = (action: string, summary: string, params: readonly string[] = [], required: readonly string[] = [], outputs: readonly string[] = [], outputRequired: readonly string[] = [], read = false, runtime = false): CapabilityRecordSource => utilityRecord({
  tool: T, action, family: 'replication', summary, params, required, outputs, outputRequired,
  effect: read ? 'read' : 'write', states: runtime ? RUNTIME : EDIT,
  supportsUndo: !runtime && !read, safeToRetry: read, dispatchAction: 'manage_networking',
});

export const NETWORKING_REPLICATION_RECORDS: readonly CapabilityRecordSource[] = [
  r('set_property_replicated', 'Set Blueprint property replication.', ['blueprintPath', 'propertyName', 'replicated', 'condition'], ['blueprintPath', 'propertyName']),
  r('set_replication_condition', 'Set a Blueprint property replication condition.', ['blueprintPath', 'propertyName', 'condition'], ['blueprintPath', 'propertyName', 'condition']),
  r('configure_net_update_frequency', 'Configure actor network update frequency.', ['blueprintPath', 'netUpdateFrequency', 'minNetUpdateFrequency'], ['blueprintPath']),
  r('configure_net_priority', 'Configure actor network bandwidth priority.', ['blueprintPath', 'netPriority'], ['blueprintPath']),
  r('set_net_dormancy', 'Set actor network dormancy.', ['blueprintPath', 'dormancy'], ['blueprintPath', 'dormancy']),
  r('configure_replication_graph', 'Configure replication graph settings.', ['blueprintPath', 'spatiallyLoaded', 'netLoadOnClient', 'replicationPolicy'], ['blueprintPath']),
  r('create_rpc_function', 'Create a Blueprint RPC function.', ['blueprintPath', 'functionName', 'rpcType', 'reliable'], ['blueprintPath', 'functionName', 'rpcType'], ['functionName'], ['functionName']),
  r('configure_rpc_validation', 'Configure RPC validation.', ['blueprintPath', 'functionName', 'withValidation'], ['blueprintPath', 'functionName']),
  r('set_rpc_reliability', 'Set RPC reliability.', ['blueprintPath', 'functionName', 'reliable'], ['blueprintPath', 'functionName', 'reliable']),
  r('set_owner', 'Set runtime actor ownership.', ['actorName', 'ownerActorName'], ['actorName'], [], [], false, true),
  r('set_autonomous_proxy', 'Configure autonomous proxy behavior.', ['blueprintPath', 'isAutonomousProxy'], ['blueprintPath']),
  r('check_has_authority', 'Read runtime actor authority state.', ['actorName'], ['actorName'], ['hasAuthority', 'role'], ['hasAuthority'], true, true),
  r('check_is_locally_controlled', 'Read local-control state for an actor.', ['actorName'], ['actorName'], ['isLocallyControlled', 'isLocalController'], ['isLocallyControlled'], true, true),
  r('configure_net_cull_distance', 'Configure network relevancy cull distance.', ['blueprintPath', 'netCullDistanceSquared', 'useOwnerNetRelevancy'], ['blueprintPath']),
  r('set_always_relevant', 'Set always-relevant replication behavior.', ['blueprintPath', 'alwaysRelevant'], ['blueprintPath']),
  r('set_only_relevant_to_owner', 'Set owner-only relevancy behavior.', ['blueprintPath', 'onlyRelevantToOwner'], ['blueprintPath']),
  r('configure_net_serialization', 'Configure custom network serialization.', ['blueprintPath', 'structName', 'customSerialization'], ['blueprintPath']),
  r('set_replicated_using', 'Assign a RepNotify function to a property.', ['blueprintPath', 'propertyName', 'repNotifyFunc'], ['blueprintPath', 'propertyName', 'repNotifyFunc']),
  r('configure_push_model', 'Configure push-model replication.', ['blueprintPath', 'usePushModel'], ['blueprintPath']),
  r('configure_client_prediction', 'Configure client-side prediction.', ['blueprintPath', 'enablePrediction', 'predictionThreshold'], ['blueprintPath']),
  r('configure_server_correction', 'Configure server correction and smoothing.', ['blueprintPath', 'correctionThreshold', 'smoothingRate'], ['blueprintPath']),
  r('add_network_prediction_data', 'Add network prediction data to a Blueprint.', ['blueprintPath', 'dataType', 'variableName'], ['blueprintPath', 'dataType']),
  r('configure_movement_prediction', 'Configure movement network prediction.', ['blueprintPath', 'networkSmoothingMode', 'networkMaxSmoothUpdateDistance', 'networkNoSmoothUpdateDistance'], ['blueprintPath']),
  r('configure_net_driver', 'Configure project net-driver settings.', ['maxClientRate', 'maxInternetClientRate', 'netServerMaxTickRate']),
  r('set_net_role', 'Set the initial Blueprint network role.', ['blueprintPath', 'role'], ['blueprintPath', 'role']),
  r('configure_replicated_movement', 'Configure replicated movement.', ['blueprintPath', 'replicateMovement'], ['blueprintPath']),
  r('get_networking_info', 'Read networking state for a Blueprint or actor.', ['blueprintPath', 'actorName'], [], ['networkingInfo'], ['networkingInfo'], true),
];
