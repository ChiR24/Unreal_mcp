// McpTool_ManageLighting.cpp — manage_lighting tool definition (15 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ManageLighting : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_lighting"); }

	FString GetDescription() const override
	{
		return TEXT("Spawn lights (point, spot, rect, sky), configure GI, shadows, "
			"volumetric fog, and build lighting.");
	}

	FString GetCategory() const override { return TEXT("world"); }


	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("spawn_light"),
				TEXT("create_light"),
				TEXT("spawn_sky_light"),
				TEXT("create_sky_light"),
				TEXT("ensure_single_sky_light"),
				TEXT("create_lightmass_volume"),
				TEXT("create_lighting_enabled_level"),
				TEXT("create_dynamic_light"),
				TEXT("setup_global_illumination"),
				TEXT("configure_shadows"),
				TEXT("set_exposure"),
				TEXT("set_ambient_occlusion"),
				TEXT("setup_volumetric_fog"),
				TEXT("build_lighting"),
				TEXT("list_light_types")
			}, TEXT("Action"))
			.String(TEXT("name"), TEXT("Name identifier."))
			.Object(TEXT("location"), TEXT("3D location (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
			})
			.StringEnum(TEXT("lightType"), {
				TEXT("Directional"),
				TEXT("Point"),
				TEXT("Spot"),
				TEXT("Rect"),
				TEXT("DirectionalLight"),
				TEXT("PointLight"),
				TEXT("SpotLight"),
				TEXT("RectLight"),
				TEXT("directional"),
				TEXT("point"),
				TEXT("spot"),
				TEXT("rect")
			}, TEXT("Light type. Accepts short names (Point), class names "
				"(PointLight), or lowercase (point)."))
			.String(TEXT("lightClass"),
				TEXT("Unreal light class name (e.g., PointLight, SpotLight). "
					"Alternative to lightType."))
			.Number(TEXT("intensity"), TEXT(""))
			.Array(TEXT("color"), TEXT("RGBA color as an array [r, g, b, a]."),
				TEXT("number"))
			.Bool(TEXT("castShadows"), TEXT(""))
			.Bool(TEXT("useAsAtmosphereSunLight"),
				TEXT("For Directional Lights, use as Atmosphere Sun Light."))
			.Number(TEXT("temperature"), TEXT(""))
			.Number(TEXT("radius"), TEXT(""))
			.Number(TEXT("falloffExponent"), TEXT(""))
			.Number(TEXT("innerCone"), TEXT(""))
			.Number(TEXT("outerCone"), TEXT(""))
			.Number(TEXT("width"), TEXT(""))
			.Number(TEXT("height"), TEXT(""))
			.StringEnum(TEXT("sourceType"), {
				TEXT("CapturedScene"),
				TEXT("SpecifiedCubemap")
			}, TEXT(""))
			.String(TEXT("cubemapPath"), TEXT("Texture asset path."))
			.Bool(TEXT("recapture"), TEXT(""))
			.StringEnum(TEXT("method"), {
				TEXT("Lightmass"),
				TEXT("LumenGI"),
				TEXT("ScreenSpace"),
				TEXT("None")
			}, TEXT(""))
			.String(TEXT("quality"), TEXT(""))
			.Number(TEXT("indirectLightingIntensity"), TEXT(""))
			.Number(TEXT("bounces"), TEXT(""))
			.String(TEXT("shadowQuality"), TEXT(""))
			.Bool(TEXT("cascadedShadows"), TEXT(""))
			.Number(TEXT("shadowDistance"), TEXT(""))
			.Bool(TEXT("contactShadows"), TEXT(""))
			.Bool(TEXT("rayTracedShadows"), TEXT(""))
			.Number(TEXT("compensationValue"), TEXT(""))
			.Number(TEXT("minBrightness"), TEXT(""))
			.Number(TEXT("maxBrightness"), TEXT(""))
			.Bool(TEXT("enabled"),
				TEXT("Whether the item/feature is enabled."))
			.Number(TEXT("density"), TEXT(""))
			.Number(TEXT("scatteringIntensity"), TEXT(""))
			.Number(TEXT("fogHeight"), TEXT(""))
			.Bool(TEXT("buildOnlySelected"), TEXT(""))
			.Bool(TEXT("buildReflectionCaptures"), TEXT(""))
			.String(TEXT("levelName"), TEXT(""))
			.Bool(TEXT("copyActors"), TEXT(""))
			.Bool(TEXT("useTemplate"), TEXT(""))
			.Object(TEXT("size"), TEXT("3D scale (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageLighting);
