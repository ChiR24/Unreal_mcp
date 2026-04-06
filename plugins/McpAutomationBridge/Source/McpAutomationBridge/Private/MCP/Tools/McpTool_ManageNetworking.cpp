// McpTool_ManageNetworking.cpp — manage_networking tool definition (27 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ManageNetworking : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_networking"); }

	FString GetDescription() const override
	{
		return TEXT("Configure multiplayer: property replication, RPCs "
			"(Server/Client/Multicast), authority, relevancy, and network prediction.");
	}

	FString GetCategory() const override { return TEXT("utility"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("set_property_replicated"),
				TEXT("set_replication_condition"),
				TEXT("configure_net_update_frequency"),
				TEXT("configure_net_priority"),
				TEXT("set_net_dormancy"),
				TEXT("configure_replication_graph"),
				TEXT("create_rpc_function"),
				TEXT("configure_rpc_validation"),
				TEXT("set_rpc_reliability"),
				TEXT("set_owner"),
				TEXT("set_autonomous_proxy"),
				TEXT("check_has_authority"),
				TEXT("check_is_locally_controlled"),
				TEXT("configure_net_cull_distance"),
				TEXT("set_always_relevant"),
				TEXT("set_only_relevant_to_owner"),
				TEXT("configure_net_serialization"),
				TEXT("set_replicated_using"),
				TEXT("configure_push_model"),
				TEXT("configure_client_prediction"),
				TEXT("configure_server_correction"),
				TEXT("add_network_prediction_data"),
				TEXT("configure_movement_prediction"),
				TEXT("configure_net_driver"),
				TEXT("set_net_role"),
				TEXT("configure_replicated_movement"),
				TEXT("get_networking_info")
			}, TEXT("Networking action to perform"))
			.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
			.String(TEXT("actorName"), TEXT("Name of the actor."))
			.String(TEXT("propertyName"), TEXT("Name of the property."))
			.Bool(TEXT("replicated"), TEXT("Whether property should be replicated."))
			.StringEnum(TEXT("condition"), {
				TEXT("COND_None"),
				TEXT("COND_InitialOnly"),
				TEXT("COND_OwnerOnly"),
				TEXT("COND_SkipOwner"),
				TEXT("COND_SimulatedOnly"),
				TEXT("COND_AutonomousOnly"),
				TEXT("COND_SimulatedOrPhysics"),
				TEXT("COND_InitialOrOwner"),
				TEXT("COND_Custom"),
				TEXT("COND_ReplayOrOwner"),
				TEXT("COND_ReplayOnly"),
				TEXT("COND_SimulatedOnlyNoReplay"),
				TEXT("COND_SimulatedOrPhysicsNoReplay"),
				TEXT("COND_SkipReplay"),
				TEXT("COND_Never")
			}, TEXT("Replication condition."))
			.String(TEXT("repNotifyFunc"), TEXT("RepNotify function name."))
			.Number(TEXT("netUpdateFrequency"), TEXT("How often actor replicates (Hz, default 100)."))
			.Number(TEXT("minNetUpdateFrequency"), TEXT("Minimum update frequency when idle (Hz, default 2)."))
			.Number(TEXT("netPriority"), TEXT("Network priority for bandwidth (default 1.0)."))
			.StringEnum(TEXT("dormancy"), {
				TEXT("DORM_Never"),
				TEXT("DORM_Awake"),
				TEXT("DORM_DormantAll"),
				TEXT("DORM_DormantPartial"),
				TEXT("DORM_Initial")
			}, TEXT("Net dormancy mode."))
			.String(TEXT("nodeClass"), TEXT("Node class path."))
			.Number(TEXT("spatialBias"), TEXT("Spatial bias for replication graph."))
			.String(TEXT("defaultSettingsClass"), TEXT("Default replication settings class."))
			.String(TEXT("functionName"), TEXT("Name of the function."))
			.StringEnum(TEXT("rpcType"), {
				TEXT("Server"),
				TEXT("Client"),
				TEXT("NetMulticast")
			}, TEXT("Type of RPC."))
			.Bool(TEXT("reliable"), TEXT("Whether the operation is reliable."))
			.ArrayOfObjects(TEXT("parameters"), TEXT("RPC function parameters."),
				[](FMcpSchemaBuilder& S) {
				S.String(TEXT("name"), TEXT("")).String(TEXT("type"), TEXT(""));
			})
			.String(TEXT("returnType"), TEXT("RPC return type (usually void)."))
			.String(TEXT("validationFunctionName"), TEXT("Name of validation function."))
			.Bool(TEXT("withValidation"), TEXT("Enable RPC validation."))
			.String(TEXT("ownerActorName"), TEXT("Name of owner actor (null to clear)."))
			.Bool(TEXT("isAutonomousProxy"), TEXT("Configure as autonomous proxy."))
			.Number(TEXT("netCullDistanceSquared"), TEXT("Network cull distance squared."))
			.Bool(TEXT("useOwnerNetRelevancy"), TEXT("Use owner relevancy."))
			.Bool(TEXT("alwaysRelevant"), TEXT("Always relevant to all clients."))
			.Bool(TEXT("onlyRelevantToOwner"), TEXT("Only relevant to owner."))
			.String(TEXT("structName"), TEXT("Name of struct for custom serialization."))
			.Bool(TEXT("useNetSerialize"), TEXT("Use custom NetSerialize."))
			.Bool(TEXT("usePushModel"), TEXT("Use push-model replication."))
			.Array(TEXT("propertyNames"), TEXT("Properties for push model."))
			.Bool(TEXT("enablePrediction"), TEXT("Enable client-side prediction."))
			.String(TEXT("predictionKey"), TEXT("Prediction key identifier."))
			.Number(TEXT("correctionThreshold"), TEXT("Server correction threshold."))
			.Number(TEXT("smoothingRate"), TEXT("Smoothing rate for corrections."))
			.StringEnum(TEXT("dataType"), {
				TEXT("InputCmd"),
				TEXT("SyncState"),
				TEXT("AuxState")
			}, TEXT("Network prediction data type."))
			.ArrayOfObjects(TEXT("properties"), TEXT("Predicted properties."),
				[](FMcpSchemaBuilder& S) {
				S.String(TEXT("name"), TEXT("")).String(TEXT("type"), TEXT(""));
			})
			.StringEnum(TEXT("networkSmoothingMode"), {
				TEXT("Disabled"),
				TEXT("Linear"),
				TEXT("Exponential")
			}, TEXT("Movement smoothing mode."))
			.Number(TEXT("networkMaxSmoothUpdateDistance"), TEXT("Max smooth update distance."))
			.Number(TEXT("networkNoSmoothUpdateDistance"), TEXT("No smooth update distance."))
			.Number(TEXT("maxClientRate"), TEXT("Max client rate."))
			.Number(TEXT("maxInternetClientRate"), TEXT("Max internet client rate."))
			.Number(TEXT("netServerMaxTickRate"), TEXT("Server max tick rate."))
			.StringEnum(TEXT("role"), {
				TEXT("ROLE_None"),
				TEXT("ROLE_SimulatedProxy"),
				TEXT("ROLE_AutonomousProxy"),
				TEXT("ROLE_Authority")
			}, TEXT("Net role."))
			.Bool(TEXT("replicateMovement"), TEXT("Replicate movement."))
			.StringEnum(TEXT("replicatedMovementMode"), {
				TEXT("Default"),
				TEXT("SkipPhysics"),
				TEXT("FullMovement")
			}, TEXT("Replicated movement mode."))
			.StringEnum(TEXT("locationQuantizationLevel"), {
				TEXT("RoundWholeNumber"),
				TEXT("RoundOneDecimal"),
				TEXT("RoundTwoDecimals")
			}, TEXT("Location quantization level."))
			.Bool(TEXT("spatiallyLoaded"), TEXT("Spatially loaded for replication graph."))
			.Bool(TEXT("netLoadOnClient"), TEXT("Net load on client for replication graph."))
			.String(TEXT("replicationPolicy"), TEXT("Replication policy for replication graph."))
			.Bool(TEXT("customSerialization"), TEXT("Use custom serialization."))
			.Number(TEXT("predictionThreshold"), TEXT("Prediction threshold for client prediction."))
			.Bool(TEXT("removeAll"), TEXT("Remove all foliage instances."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageNetworking);
