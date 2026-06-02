// McpTool_ManageCombat.cpp — manage_combat tool definition (39 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ManageCombat : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_combat"); }

	FString GetDescription() const override
	{
		return TEXT("Create weapons with hitscan/projectile firing, configure damage types, "
			"hitboxes, reload, and melee combat (combos, parry, block).");
	}

	FString GetCategory() const override { return TEXT("gameplay"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("create_weapon_blueprint"),
				TEXT("configure_weapon_mesh"),
				TEXT("configure_weapon_sockets"),
				TEXT("set_weapon_stats"),
				TEXT("configure_hitscan"),
				TEXT("configure_projectile"),
				TEXT("configure_spread_pattern"),
				TEXT("configure_recoil_pattern"),
				TEXT("configure_aim_down_sights"),
				TEXT("create_projectile_blueprint"),
				TEXT("configure_projectile_movement"),
				TEXT("configure_projectile_collision"),
				TEXT("configure_projectile_homing"),
				TEXT("create_damage_type"),
				TEXT("configure_damage_execution"),
				TEXT("setup_hitbox_component"),
				TEXT("setup_reload_system"),
				TEXT("setup_ammo_system"),
				TEXT("setup_attachment_system"),
				TEXT("setup_weapon_switching"),
				TEXT("configure_muzzle_flash"),
				TEXT("configure_tracer"),
				TEXT("configure_impact_effects"),
				TEXT("configure_shell_ejection"),
				TEXT("create_melee_trace"),
				TEXT("configure_combo_system"),
				TEXT("create_hit_pause"),
				TEXT("configure_hit_reaction"),
				TEXT("setup_parry_block_system"),
				TEXT("configure_weapon_trails"),
				TEXT("get_combat_info"),
				TEXT("setup_damage_type"),
				TEXT("configure_hit_detection"),
				TEXT("get_combat_stats"),
				TEXT("create_damage_effect"),
				TEXT("apply_damage"),
				TEXT("heal"),
				TEXT("create_shield"),
				TEXT("modify_armor")
			}, TEXT("Combat action to perform"))
			.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("path"), TEXT("Directory path for asset creation."))
			.String(TEXT("weaponMeshPath"), TEXT("Path to weapon static/skeletal mesh."))
			.String(TEXT("muzzleSocketName"), TEXT("Muzzle socket name."))
			.String(TEXT("ejectionSocketName"), TEXT("Shell ejection socket name."))
			.Number(TEXT("baseDamage"), TEXT(""))
			.Number(TEXT("fireRate"), TEXT(""))
			.Number(TEXT("range"), TEXT(""))
			.Number(TEXT("spread"), TEXT(""))
			.Bool(TEXT("hitscanEnabled"), TEXT("Enable hitscan firing."))
			.StringEnum(TEXT("traceChannel"), {
				TEXT("Visibility"),
				TEXT("Camera"),
				TEXT("Weapon"),
				TEXT("Custom")
			}, TEXT("Trace channel for hitscan."))
			.String(TEXT("projectileClass"), TEXT("Projectile class path."))
			.StringEnum(TEXT("spreadPattern"), {
				TEXT("Random"),
				TEXT("Fixed"),
				TEXT("FixedWithRandom"),
				TEXT("Shotgun")
			}, TEXT("Spread pattern type."))
			.Number(TEXT("spreadIncrease"), TEXT("Spread increase per shot."))
			.Number(TEXT("spreadRecovery"), TEXT("Spread recovery rate."))
			.Number(TEXT("recoilPitch"), TEXT("Vertical recoil (degrees)."))
			.Number(TEXT("recoilYaw"), TEXT("Horizontal recoil (degrees)."))
			.Number(TEXT("recoilRecovery"), TEXT("Recoil recovery speed."))
			.Bool(TEXT("adsEnabled"), TEXT("Enable aim down sights."))
			.Number(TEXT("adsFov"), TEXT("FOV when aiming."))
			.Number(TEXT("adsSpeed"), TEXT("Time to aim down sights."))
			.Number(TEXT("adsSpreadMultiplier"), TEXT("Spread multiplier when aiming."))
			.Number(TEXT("projectileSpeed"), TEXT(""))
			.Number(TEXT("projectileGravityScale"), TEXT(""))
			.Number(TEXT("projectileLifespan"), TEXT(""))
			.String(TEXT("projectileMeshPath"), TEXT("Path to projectile mesh."))
			.Number(TEXT("collisionRadius"), TEXT(""))
			.Bool(TEXT("bounceEnabled"), TEXT("Enable projectile bouncing."))
			.Number(TEXT("bounceVelocityRatio"), TEXT("Velocity retained on bounce (0-1)."))
			.Bool(TEXT("homingEnabled"), TEXT("Enable homing behavior."))
			.Number(TEXT("homingAcceleration"), TEXT("Homing turn rate."))
			.Number(TEXT("damageImpulse"), TEXT("Impulse applied on hit."))
			.Number(TEXT("criticalMultiplier"), TEXT("Critical hit damage multiplier."))
			.Number(TEXT("headshotMultiplier"), TEXT("Headshot damage multiplier."))
			.String(TEXT("hitboxBoneName"), TEXT("Bone name for hitbox."))
			.StringEnum(TEXT("hitboxType"), {
				TEXT("Capsule"),
				TEXT("Box"),
				TEXT("Sphere")
			}, TEXT("Hitbox collision shape."))
			.Object(TEXT("hitboxSize"), TEXT("Hitbox dimensions."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("radius"))
				 .Number(TEXT("halfHeight"))
				 .Object(TEXT("extent"), TEXT("3D extent (half-size)."),
					[](FMcpSchemaBuilder& Inner) {
					Inner.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
				});
			})
			.Bool(TEXT("isDamageZoneHead"), TEXT("Mark as headshot zone."))
			.Number(TEXT("damageMultiplier"), TEXT("Damage multiplier for this hitbox."))
			.Number(TEXT("magazineSize"), TEXT(""))
			.Number(TEXT("reloadTime"), TEXT(""))
			.String(TEXT("reloadAnimationPath"), TEXT("Path to reload animation."))
			.String(TEXT("ammoType"), TEXT("Ammo type identifier."))
			.Number(TEXT("maxAmmo"), TEXT(""))
			.Number(TEXT("startingAmmo"), TEXT(""))
			.ArrayOfObjects(TEXT("attachmentSlots"), TEXT("Attachment slot definitions."))
			.Number(TEXT("switchInTime"), TEXT("Time to equip weapon."))
			.Number(TEXT("switchOutTime"), TEXT("Time to unequip weapon."))
			.String(TEXT("equipAnimationPath"), TEXT("Path to equip animation montage."))
			.String(TEXT("unequipAnimationPath"), TEXT("Path to unequip animation montage."))
			.String(TEXT("muzzleFlashParticlePath"), TEXT("Path to muzzle flash particle."))
			.Number(TEXT("muzzleFlashScale"), TEXT("Muzzle flash scale."))
			.String(TEXT("muzzleSoundPath"), TEXT("Path to firing sound."))
			.String(TEXT("tracerParticlePath"), TEXT("Path to tracer particle."))
			.Number(TEXT("tracerSpeed"), TEXT("Tracer travel speed."))
			.String(TEXT("impactParticlePath"), TEXT("Path to impact particle."))
			.String(TEXT("impactSoundPath"), TEXT("Path to impact sound."))
			.String(TEXT("impactDecalPath"), TEXT("Path to impact decal."))
			.String(TEXT("shellMeshPath"), TEXT("Path to shell casing mesh."))
			.Number(TEXT("shellEjectionForce"), TEXT("Shell ejection impulse."))
			.Number(TEXT("shellLifespan"), TEXT("Shell casing lifetime."))
			.String(TEXT("meleeTraceStartSocket"), TEXT("Socket for trace start."))
			.String(TEXT("meleeTraceEndSocket"), TEXT("Socket for trace end."))
			.Number(TEXT("meleeTraceRadius"), TEXT("Sphere trace radius."))
			.Number(TEXT("comboWindowTime"), TEXT("Time window for combo input."))
			.Number(TEXT("maxComboCount"), TEXT("Maximum combo length."))
			.Number(TEXT("hitPauseDuration"), TEXT("Hitstop duration in seconds."))
			.Number(TEXT("hitPauseTimeDilation"), TEXT("Time dilation during hitstop."))
			.String(TEXT("hitReactionMontage"), TEXT("Path to hit reaction montage."))
			.Number(TEXT("hitReactionStunTime"), TEXT("Stun duration on hit."))
			.Number(TEXT("parryWindowStart"), TEXT("Parry window start time (normalized)."))
			.Number(TEXT("parryWindowEnd"), TEXT("Parry window end time (normalized)."))
			.String(TEXT("parryAnimationPath"), TEXT("Path to parry animation."))
			.Number(TEXT("blockDamageReduction"), TEXT("Damage reduction when blocking (0-1)."))
			.Number(TEXT("blockStaminaCost"), TEXT("Stamina cost per blocked hit."))
			.String(TEXT("weaponTrailParticlePath"), TEXT("Path to weapon trail particle."))
			.String(TEXT("weaponTrailStartSocket"), TEXT("Trail start socket."))
			.String(TEXT("weaponTrailEndSocket"), TEXT("Trail end socket."))
			.Number(TEXT("ammoPerShot"), TEXT("Ammo consumed per shot."))
			.Number(TEXT("armorValue"), TEXT("Armor value to apply."))
			.Number(TEXT("damageAmount"), TEXT("Damage amount to apply."))
			.Number(TEXT("damagePerSecond"), TEXT("Damage per second for damage effects."))
			.Number(TEXT("damageReduction"), TEXT("Armor damage reduction multiplier."))
			.String(TEXT("damageType"), TEXT("Damage type identifier."))
			.Number(TEXT("duration"), TEXT("Effect duration in seconds."))
			.String(TEXT("effectType"), TEXT("Damage effect type."))
			.Number(TEXT("healAmount"), TEXT("Healing amount to apply."))
			.Bool(TEXT("infiniteAmmo"), TEXT("Whether ammo is infinite."))
			.Number(TEXT("maxHealth"), TEXT("Maximum health value."))
			.Number(TEXT("maxShield"), TEXT("Maximum shield value."))
			.Number(TEXT("shieldAmount"), TEXT("Current shield amount."))
			.Number(TEXT("shieldRegenDelay"), TEXT("Delay before shield regeneration."))
			.Number(TEXT("shieldRegenRate"), TEXT("Shield regeneration rate."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageCombat);
