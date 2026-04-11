// McpTool_ManageAI.cpp — manage_ai tool definition (44 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ManageAI : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_ai"); }

	FString GetDescription() const override
	{
		return TEXT("Create AI Controllers, configure Behavior Trees, "
			"Blackboards, EQS queries, and perception systems.");
	}

	FString GetCategory() const override { return TEXT("gameplay"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("create_ai_controller"),
				TEXT("assign_behavior_tree"),
				TEXT("assign_blackboard"),
				TEXT("create_blackboard_asset"),
				TEXT("add_blackboard_key"),
				TEXT("set_key_instance_synced"),
				TEXT("create_behavior_tree"),
				TEXT("add_composite_node"),
				TEXT("add_task_node"),
				TEXT("add_decorator"),
				TEXT("add_service"),
				TEXT("configure_bt_node"),
				TEXT("create_eqs_query"),
				TEXT("add_eqs_generator"),
				TEXT("add_eqs_context"),
				TEXT("add_eqs_test"),
				TEXT("configure_test_scoring"),
				TEXT("add_ai_perception_component"),
				TEXT("configure_sight_config"),
				TEXT("configure_hearing_config"),
				TEXT("configure_damage_sense_config"),
				TEXT("set_perception_team"),
				TEXT("create_state_tree"),
				TEXT("add_state_tree_state"),
				TEXT("add_state_tree_transition"),
				TEXT("configure_state_tree_task"),
				TEXT("create_smart_object_definition"),
				TEXT("add_smart_object_slot"),
				TEXT("configure_slot_behavior"),
				TEXT("add_smart_object_component"),
				TEXT("create_mass_entity_config"),
				TEXT("configure_mass_entity"),
				TEXT("add_mass_spawner"),
				TEXT("get_ai_info"),
				TEXT("create_blackboard"),
				TEXT("setup_perception"),
				TEXT("create_nav_link_proxy"),
				TEXT("set_focus"),
				TEXT("clear_focus"),
				TEXT("set_blackboard_value"),
				TEXT("get_blackboard_value"),
				TEXT("run_behavior_tree"),
				TEXT("stop_behavior_tree")
			}, TEXT("AI action to perform"))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("path"), TEXT("Directory path for asset creation."))
			.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
			.String(TEXT("controllerPath"), TEXT("Path to controller blueprint."))
			.String(TEXT("behaviorTreePath"), TEXT("Path to behavior tree asset."))
			.String(TEXT("blackboardPath"), TEXT("Path to blackboard asset."))
			.StringEnum(TEXT("parentClass"), {
				TEXT("AAIController"),
				TEXT("APlayerController")
			}, TEXT("Parent class for AI controller (default: AAIController)."))
			.Bool(TEXT("autoRunBehaviorTree"), TEXT("Start behavior tree automatically on possess."))
			.String(TEXT("keyName"), TEXT("Name of the key."))
			.StringEnum(TEXT("keyType"), {
				TEXT("Bool"),
				TEXT("Int"),
				TEXT("Float"),
				TEXT("Vector"),
				TEXT("Rotator"),
				TEXT("Object"),
				TEXT("Class"),
				TEXT("Enum"),
				TEXT("Name"),
				TEXT("String")
			}, TEXT("Blackboard key data type."))
			.Bool(TEXT("isInstanceSynced"), TEXT("Sync key across instances."))
			.String(TEXT("baseObjectClass"), TEXT("Base class for Object/Class keys."))
			.String(TEXT("enumClass"), TEXT("Enum class for Enum keys."))
			.StringEnum(TEXT("compositeType"), {
				TEXT("Selector"),
				TEXT("Sequence"),
				TEXT("Parallel"),
				TEXT("SimpleParallel")
			}, TEXT("Composite node type."))
			.StringEnum(TEXT("taskType"), {
				TEXT("MoveTo"),
				TEXT("MoveDirectlyToward"),
				TEXT("RotateToFaceBBEntry"),
				TEXT("Wait"),
				TEXT("WaitBlackboardTime"),
				TEXT("PlayAnimation"),
				TEXT("PlaySound"),
				TEXT("RunEQSQuery"),
				TEXT("RunBehaviorDynamic"),
				TEXT("SetBlackboardValue"),
				TEXT("PushPawnAction"),
				TEXT("FinishWithResult"),
				TEXT("MakeNoise"),
				TEXT("GameplayTaskBase"),
				TEXT("Custom")
			}, TEXT("Task node type."))
			.StringEnum(TEXT("decoratorType"), {
				TEXT("Blackboard"),
				TEXT("BlackboardBased"),
				TEXT("CompareBBEntries"),
				TEXT("Cooldown"),
				TEXT("ConeCheck"),
				TEXT("DoesPathExist"),
				TEXT("IsAtLocation"),
				TEXT("IsBBEntryOfClass"),
				TEXT("KeepInCone"),
				TEXT("Loop"),
				TEXT("SetTagCooldown"),
				TEXT("TagCooldown"),
				TEXT("TimeLimit"),
				TEXT("ForceSuccess"),
				TEXT("ConditionalLoop"),
				TEXT("Custom")
			}, TEXT("Decorator node type."))
			.StringEnum(TEXT("serviceType"), {
				TEXT("DefaultFocus"),
				TEXT("RunEQS"),
				TEXT("Custom")
			}, TEXT("Service node type."))
			.String(TEXT("parentNodeId"), TEXT("ID of the node."))
			.String(TEXT("nodeId"), TEXT("ID of the node."))
			.FreeformObject(TEXT("nodeProperties"), TEXT("Properties to set on the node."))
			.String(TEXT("customTaskClass"), TEXT("Custom task class path for Custom task type."))
			.String(TEXT("customDecoratorClass"), TEXT("Custom decorator class path."))
			.String(TEXT("customServiceClass"), TEXT("Custom service class path."))
			.String(TEXT("queryPath"), TEXT("Path to EQS query asset."))
			.StringEnum(TEXT("generatorType"), {
				TEXT("ActorsOfClass"),
				TEXT("CurrentLocation"),
				TEXT("Donut"),
				TEXT("OnCircle"),
				TEXT("PathingGrid"),
				TEXT("SimpleGrid"),
				TEXT("Composite"),
				TEXT("Custom")
			}, TEXT("EQS generator type."))
			.StringEnum(TEXT("contextType"), {
				TEXT("Querier"),
				TEXT("Item"),
				TEXT("EnvQueryContext_BlueprintBase"),
				TEXT("Custom")
			}, TEXT("EQS context type."))
			.StringEnum(TEXT("testType"), {
				TEXT("Distance"),
				TEXT("Dot"),
				TEXT("GameplayTags"),
				TEXT("Overlap"),
				TEXT("Pathfinding"),
				TEXT("PathfindingBatch"),
				TEXT("Project"),
				TEXT("Random"),
				TEXT("Trace"),
				TEXT("Custom")
			}, TEXT("EQS test type."))
			.Object(TEXT("generatorSettings"), TEXT("Generator-specific settings."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("searchRadius"))
				 .String(TEXT("searchCenter"), TEXT(""))
				 .String(TEXT("actorClass"), TEXT(""))
				 .Number(TEXT("gridSize"))
				 .Number(TEXT("spacesBetween"))
				 .Number(TEXT("innerRadius"))
				 .Number(TEXT("outerRadius"));
			})
			.Object(TEXT("testSettings"), TEXT("Test scoring and filter settings."),
				[](FMcpSchemaBuilder& S) {
				S.StringEnum(TEXT("scoringEquation"), {
					TEXT("Linear"),
					TEXT("Square"),
					TEXT("InverseLinear"),
					TEXT("Constant")
				}, TEXT(""))
				 .Number(TEXT("clampMin"))
				 .Number(TEXT("clampMax"))
				 .StringEnum(TEXT("filterType"), {
					TEXT("Minimum"),
					TEXT("Maximum"),
					TEXT("Range")
				}, TEXT(""))
				 .Number(TEXT("floatMin"))
				 .Number(TEXT("floatMax"));
			})
			.Number(TEXT("testIndex"), TEXT("Index of test to configure."))
			.Object(TEXT("sightConfig"), TEXT("AI sight sense configuration."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("sightRadius"))
				 .Number(TEXT("loseSightRadius"))
				 .Number(TEXT("peripheralVisionAngle"))
				 .Number(TEXT("pointOfViewBackwardOffset"))
				 .Number(TEXT("nearClippingRadius"))
				 .Number(TEXT("autoSuccessRange"))
				 .Number(TEXT("maxAge"))
				 .Object(TEXT("detectionByAffiliation"), TEXT(""),
					[](FMcpSchemaBuilder& Inner) {
					Inner.Bool(TEXT("enemies"), TEXT(""))
						 .Bool(TEXT("neutrals"), TEXT(""))
						 .Bool(TEXT("friendlies"), TEXT(""));
				});
			})
			.Object(TEXT("hearingConfig"), TEXT("AI hearing sense configuration."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("hearingRange"))
				 .Number(TEXT("loSHearingRange"))
				 .Bool(TEXT("detectFriendly"), TEXT(""))
				 .Number(TEXT("maxAge"));
			})
			.Object(TEXT("damageConfig"), TEXT("AI damage sense configuration."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("maxAge"));
			})
			.Number(TEXT("teamId"), TEXT("Team ID for perception affiliation (0=Neutral, 1=Player, 2=Enemy, etc.)."))
			.StringEnum(TEXT("dominantSense"), {
				TEXT("Sight"),
				TEXT("Hearing"),
				TEXT("Damage"),
				TEXT("Touch"),
				TEXT("None")
			}, TEXT("Dominant sense for perception prioritization."))
			.String(TEXT("stateTreePath"), TEXT("Path to State Tree asset."))
			.String(TEXT("stateName"), TEXT("Name of the state."))
			.String(TEXT("fromState"), TEXT("Source state name."))
			.String(TEXT("toState"), TEXT("Target state name."))
			.String(TEXT("transitionCondition"), TEXT("Condition expression for transition."))
			.String(TEXT("stateTaskClass"), TEXT("Task class for state."))
			.String(TEXT("stateEvaluatorClass"), TEXT("Evaluator class for state."))
			.String(TEXT("definitionPath"), TEXT("Path to definition asset."))
			.Number(TEXT("slotIndex"), TEXT("Index of slot to configure."))
			.Object(TEXT("slotOffset"), TEXT("Local offset for slot."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("slotRotation"), TEXT("Local rotation for slot."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
			})
			.String(TEXT("slotBehaviorDefinition"), TEXT("Gameplay behavior definition for slot."))
			.Array(TEXT("slotActivityTags"), TEXT("Activity tags for the slot."))
			.Array(TEXT("slotUserTags"), TEXT("Required user tags for slot."))
			.Bool(TEXT("slotEnabled"), TEXT("Whether slot is enabled."))
			.String(TEXT("configPath"), TEXT("Path to config asset."))
			.Array(TEXT("massTraits"), TEXT("List of Mass traits to add."))
			.Array(TEXT("massProcessors"), TEXT("List of Mass processors to configure."))
			.Object(TEXT("spawnerSettings"), TEXT("Mass spawner configuration."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("entityCount"))
				 .Number(TEXT("spawnRadius"))
				 .String(TEXT("entityConfig"), TEXT(""))
				 .Bool(TEXT("spawnOnBeginPlay"), TEXT(""));
			})
			.FreeformObject(TEXT("value"), TEXT("Generic value (any type)."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageAI);
