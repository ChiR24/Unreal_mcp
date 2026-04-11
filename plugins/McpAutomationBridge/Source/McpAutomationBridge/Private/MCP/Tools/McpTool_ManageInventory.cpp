// McpTool_ManageInventory.cpp — manage_inventory tool definition (27 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ManageInventory : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_inventory"); }

	FString GetDescription() const override
	{
		return TEXT("Create item data assets, inventory components, "
			"world pickups, loot tables, and crafting recipes.");
	}

	FString GetCategory() const override { return TEXT("gameplay"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("create_item_data_asset"),
				TEXT("set_item_properties"),
				TEXT("create_item_category"),
				TEXT("assign_item_category"),
				TEXT("create_inventory_component"),
				TEXT("configure_inventory_slots"),
				TEXT("add_inventory_functions"),
				TEXT("configure_inventory_events"),
				TEXT("set_inventory_replication"),
				TEXT("create_pickup_actor"),
				TEXT("configure_pickup_interaction"),
				TEXT("configure_pickup_respawn"),
				TEXT("configure_pickup_effects"),
				TEXT("create_equipment_component"),
				TEXT("define_equipment_slots"),
				TEXT("configure_equipment_effects"),
				TEXT("add_equipment_functions"),
				TEXT("configure_equipment_visuals"),
				TEXT("create_loot_table"),
				TEXT("add_loot_entry"),
				TEXT("configure_loot_drop"),
				TEXT("set_loot_quality_tiers"),
				TEXT("create_crafting_recipe"),
				TEXT("configure_recipe_requirements"),
				TEXT("create_crafting_station"),
				TEXT("add_crafting_component"),
				TEXT("get_inventory_info")
			}, TEXT("Inventory action to perform."))
			.String(TEXT("name"), TEXT("Name of the asset to create."))
			.String(TEXT("path"), TEXT("Directory path for asset creation."))
			.String(TEXT("folder"), TEXT("Path to a directory."))
			.Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
			.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
			.String(TEXT("itemPath"), TEXT("Path to item data asset."))
			.String(TEXT("parentClass"), TEXT("Path or name of the parent class."))
			.String(TEXT("displayName"), TEXT(""))
			.String(TEXT("description"), TEXT(""))
			.String(TEXT("icon"), TEXT("Path to icon texture."))
			.String(TEXT("mesh"), TEXT("Path to mesh asset."))
			.Number(TEXT("stackSize"), TEXT(""))
			.Number(TEXT("weight"), TEXT(""))
			.StringEnum(TEXT("rarity"), {
				TEXT("Common"),
				TEXT("Uncommon"),
				TEXT("Rare"),
				TEXT("Epic"),
				TEXT("Legendary"),
				TEXT("Custom")
			}, TEXT("Item rarity tier."))
			.Number(TEXT("value"), TEXT(""))
			.Array(TEXT("tags"), TEXT("Gameplay tags for item categorization."))
			.FreeformObject(TEXT("customProperties"), TEXT("Custom key-value properties for item."))
			.String(TEXT("categoryPath"), TEXT("Path to item category asset."))
			.String(TEXT("parentCategory"), TEXT("Parent category path."))
			.String(TEXT("categoryIcon"), TEXT("Icon texture for category."))
			.String(TEXT("componentName"), TEXT("Name of the component."))
			.Number(TEXT("slotCount"), TEXT(""))
			.Object(TEXT("slotSize"), TEXT("Size of each slot (for grid inventory)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("width")).Number(TEXT("height"));
			})
			.Number(TEXT("maxWeight"), TEXT(""))
			.Bool(TEXT("allowStacking"), TEXT("Allow items to stack."))
			.Array(TEXT("slotCategories"), TEXT("Allowed item categories per slot."))
			.ArrayOfObjects(TEXT("slotRestrictions"), TEXT("Per-slot category restrictions."))
			.Bool(TEXT("replicated"), TEXT("Whether to replicate."))
			.StringEnum(TEXT("replicationCondition"), {
				TEXT("None"),
				TEXT("OwnerOnly"),
				TEXT("SkipOwner"),
				TEXT("SimulatedOnly"),
				TEXT("AutonomousOnly"),
				TEXT("Custom")
			}, TEXT("Replication condition for inventory."))
			.String(TEXT("pickupPath"), TEXT("Path to pickup actor Blueprint."))
			.String(TEXT("meshPath"), TEXT("Mesh asset path."))
			.String(TEXT("itemDataPath"), TEXT("Path to item data asset."))
			.Number(TEXT("interactionRadius"), TEXT("Radius for pickup interaction."))
			.StringEnum(TEXT("interactionType"), {
				TEXT("Overlap"),
				TEXT("Interact"),
				TEXT("Key"),
				TEXT("Hold")
			}, TEXT("How player picks up item."))
			.String(TEXT("interactionKey"), TEXT("Input action for pickup (if type is Key/Hold)."))
			.String(TEXT("prompt"), TEXT("Prompt text."))
			.String(TEXT("highlightMaterial"), TEXT("Material for highlight effect."))
			.Bool(TEXT("respawnable"), TEXT(""))
			.Number(TEXT("respawnTime"), TEXT("Respawn time in seconds."))
			.String(TEXT("respawnEffect"), TEXT("Niagara effect for respawn."))
			.String(TEXT("pickupSound"), TEXT("Sound cue for pickup."))
			.String(TEXT("pickupParticle"), TEXT("Particle effect on pickup."))
			.Bool(TEXT("bobbing"), TEXT("Enable bobbing animation."))
			.Bool(TEXT("rotation"), TEXT("Enable rotation animation."))
			.Bool(TEXT("glowEffect"), TEXT("Enable glow effect."))
			.ArrayOfObjects(TEXT("slots"), TEXT("Equipment slot definitions."))
			.ArrayOfObjects(TEXT("statModifiers"), TEXT("Stat modifiers when equipped."))
			.Array(TEXT("abilityGrants"), TEXT("Gameplay abilities granted when equipped."))
			.Array(TEXT("passiveEffects"), TEXT("Passive gameplay effects when equipped."))
			.Bool(TEXT("attachToSocket"), TEXT("Attach mesh to socket when equipped."))
			.String(TEXT("meshComponent"), TEXT("Component name for equipment mesh."))
			.FreeformObject(TEXT("animationOverrides"), TEXT("Animation overrides (slot -> anim asset)."))
			.String(TEXT("lootTablePath"), TEXT("Path to loot table asset."))
			.Number(TEXT("lootWeight"), TEXT("Weight for drop chance calculation."))
			.Number(TEXT("minQuantity"), TEXT("Minimum drop quantity."))
			.Number(TEXT("maxQuantity"), TEXT("Maximum drop quantity."))
			.Array(TEXT("conditions"), TEXT("Conditions for loot entry (gameplay tag expressions)."))
			.String(TEXT("actorPath"), TEXT("Path to actor Blueprint for loot drop."))
			.Number(TEXT("dropCount"), TEXT("Number of drops to roll."))
			.Array(TEXT("guaranteedDrops"), TEXT("Item paths that always drop."))
			.Number(TEXT("dropRadius"), TEXT("Radius for scattered drops."))
			.Number(TEXT("dropForce"), TEXT("Physics force applied to drops."))
			.ArrayOfObjects(TEXT("tiers"), TEXT("Quality tier definitions."))
			.String(TEXT("recipePath"), TEXT("Path to crafting recipe asset."))
			.String(TEXT("outputItemPath"), TEXT("Path to item produced by recipe."))
			.Number(TEXT("outputQuantity"), TEXT("Quantity produced."))
			.ArrayOfObjects(TEXT("ingredients"), TEXT("Required ingredients with quantities."))
			.Number(TEXT("craftTime"), TEXT("Time in seconds to craft."))
			.Number(TEXT("requiredLevel"), TEXT("Required player level."))
			.Array(TEXT("requiredSkills"), TEXT("Required skill tags."))
			.String(TEXT("requiredStation"), TEXT("Required crafting station type."))
			.Array(TEXT("unlockConditions"), TEXT("Conditions to unlock recipe."))
			.Array(TEXT("recipes"), TEXT("Recipe paths for crafting station."))
			.String(TEXT("stationType"), TEXT("Type of crafting station."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageInventory);
