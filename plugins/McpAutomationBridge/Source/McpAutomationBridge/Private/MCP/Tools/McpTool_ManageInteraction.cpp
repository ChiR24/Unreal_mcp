// McpTool_ManageInteraction.cpp — manage_interaction tool definition (22 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ManageInteraction : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_interaction"); }

	FString GetDescription() const override
	{
		return TEXT("Create interactive objects: doors, switches, chests, levers. "
			"Set up destructible meshes and trigger volumes.");
	}

	FString GetCategory() const override { return TEXT("gameplay"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("create_interaction_component"),
				TEXT("configure_interaction_trace"),
				TEXT("configure_interaction_widget"),
				TEXT("add_interaction_events"),
				TEXT("create_interactable_interface"),
				TEXT("create_door_actor"),
				TEXT("configure_door_properties"),
				TEXT("create_switch_actor"),
				TEXT("configure_switch_properties"),
				TEXT("create_chest_actor"),
				TEXT("configure_chest_properties"),
				TEXT("create_lever_actor"),
				TEXT("setup_destructible_mesh"),
				TEXT("configure_destruction_levels"),
				TEXT("configure_destruction_effects"),
				TEXT("configure_destruction_damage"),
				TEXT("add_destruction_component"),
				TEXT("create_trigger_actor"),
				TEXT("configure_trigger_events"),
				TEXT("configure_trigger_filter"),
				TEXT("configure_trigger_response"),
				TEXT("get_interaction_info")
			}, TEXT("The interaction action to perform."))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("folder"), TEXT("Path to a directory."))
			.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
			.String(TEXT("actorName"), TEXT("Name of the actor."))
			.String(TEXT("componentName"), TEXT("Name of the component."))
			.StringEnum(TEXT("traceType"), {
				TEXT("line"),
				TEXT("sphere"),
				TEXT("box")
			}, TEXT("Type of interaction trace."))
			.String(TEXT("traceChannel"), TEXT("Collision trace channel."))
			.Number(TEXT("traceDistance"), TEXT("Trace distance."))
			.Number(TEXT("traceRadius"), TEXT("Trace radius."))
			.Number(TEXT("traceFrequency"), TEXT("Trace frequency."))
			.String(TEXT("widgetClass"), TEXT("Widget class path."))
			.Object(TEXT("widgetOffset"), TEXT("Widget offset from actor."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Bool(TEXT("showOnHover"), TEXT("Show widget when hovering."))
			.Bool(TEXT("showPromptText"), TEXT("Show interaction prompt text."))
			.String(TEXT("promptTextFormat"), TEXT("Format string for prompt (e.g., \"Press {Key} to {Action}\")."))
			.String(TEXT("doorPath"), TEXT("Path to door actor blueprint."))
			.String(TEXT("meshPath"), TEXT("Mesh asset path."))
			.Number(TEXT("openAngle"), TEXT("Door open rotation angle in degrees."))
			.Number(TEXT("openTime"), TEXT("Time to open/close door in seconds."))
			.StringEnum(TEXT("openDirection"), {
				TEXT("push"),
				TEXT("pull"),
				TEXT("auto")
			}, TEXT("Door open direction."))
			.Object(TEXT("pivotOffset"), TEXT("Offset for door pivot point."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Bool(TEXT("locked"), TEXT("Whether the item is locked."))
			.String(TEXT("keyItemPath"), TEXT("Item required to unlock."))
			.String(TEXT("openSound"), TEXT("Sound to play on open."))
			.String(TEXT("closeSound"), TEXT("Sound to play on close."))
			.Bool(TEXT("autoClose"), TEXT("Automatically close after opening."))
			.Number(TEXT("autoCloseDelay"), TEXT("Delay before auto-close in seconds."))
			.Bool(TEXT("requiresKey"), TEXT("Whether interaction requires a key item."))
			.String(TEXT("switchPath"), TEXT("Path to switch actor blueprint."))
			.StringEnum(TEXT("switchType"), {
				TEXT("button"),
				TEXT("lever"),
				TEXT("pressure_plate"),
				TEXT("toggle")
			}, TEXT("Type of switch."))
			.Bool(TEXT("toggleable"), TEXT("Whether switch can be toggled."))
			.Bool(TEXT("oneShot"), TEXT("Whether switch can only be used once."))
			.Number(TEXT("resetTime"), TEXT("Time to reset switch in seconds."))
			.String(TEXT("activateSound"), TEXT("Sound on activation."))
			.String(TEXT("deactivateSound"), TEXT("Sound on deactivation."))
			.Array(TEXT("targetActors"), TEXT("Actors affected by this switch."))
			.String(TEXT("chestPath"), TEXT("Path to chest actor blueprint."))
			.String(TEXT("lidMeshPath"), TEXT("Path to lid mesh."))
			.String(TEXT("lootTablePath"), TEXT("Path to loot table asset."))
			.Bool(TEXT("respawnable"), TEXT(""))
			.Number(TEXT("respawnTime"), TEXT("Respawn time in seconds."))
			.StringEnum(TEXT("leverType"), {
				TEXT("rotate"),
				TEXT("translate")
			}, TEXT("Lever movement type."))
			.Number(TEXT("moveDistance"), TEXT("Distance for translation lever."))
			.Number(TEXT("moveTime"), TEXT("Time for lever movement."))
			.StringEnum(TEXT("fractureMode"), {
				TEXT("voronoi"),
				TEXT("uniform"),
				TEXT("radial"),
				TEXT("custom")
			}, TEXT("Fracture pattern type."))
			.Number(TEXT("fracturePieces"), TEXT("Number of fracture pieces."))
			.Bool(TEXT("enablePhysics"), TEXT("Enable physics on destruction."))
			.ArrayOfObjects(TEXT("levels"), TEXT("Destruction level definitions."))
			.String(TEXT("destroySound"), TEXT("Sound on destruction."))
			.String(TEXT("destroyParticle"), TEXT("Particle effect on destruction."))
			.String(TEXT("debrisPhysicsMaterial"), TEXT("Physics material for debris."))
			.Number(TEXT("debrisLifetime"), TEXT("Debris lifetime in seconds."))
			.Number(TEXT("maxHealth"), TEXT("Maximum health before destruction."))
			.Array(TEXT("damageThresholds"), TEXT("Damage thresholds for destruction levels."), TEXT("number"))
			.Number(TEXT("impactDamageMultiplier"), TEXT("Multiplier for impact damage."))
			.Number(TEXT("radialDamageMultiplier"), TEXT("Multiplier for radial damage."))
			.Bool(TEXT("autoDestroy"), TEXT("Automatically destroy at zero health."))
			.String(TEXT("triggerPath"), TEXT("Path to trigger actor blueprint."))
			.StringEnum(TEXT("triggerShape"), {
				TEXT("box"),
				TEXT("sphere"),
				TEXT("capsule")
			}, TEXT("Shape of trigger volume."))
			.Object(TEXT("size"), TEXT("Size of trigger volume."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.String(TEXT("filterByTag"), TEXT("Actor tag filter for trigger."))
			.String(TEXT("filterByClass"), TEXT("Actor class filter for trigger."))
			.String(TEXT("filterByInterface"), TEXT("Interface filter for trigger."))
			.Array(TEXT("ignoreClasses"), TEXT("Classes to ignore in trigger."))
			.Array(TEXT("ignoreTags"), TEXT("Tags to ignore in trigger."))
			.String(TEXT("onEnterEvent"), TEXT("Event dispatcher name for enter."))
			.String(TEXT("onExitEvent"), TEXT("Event dispatcher name for exit."))
			.String(TEXT("onStayEvent"), TEXT("Event dispatcher name for stay."))
			.Number(TEXT("stayInterval"), TEXT("Interval for stay events in seconds."))
			.StringEnum(TEXT("responseType"), {
				TEXT("once"),
				TEXT("repeatable"),
				TEXT("toggle")
			}, TEXT("How trigger responds."))
			.Number(TEXT("cooldown"), TEXT("Cooldown time in seconds."))
			.Number(TEXT("maxActivations"), TEXT("Maximum number of activations (0 = unlimited)."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageInteraction);
