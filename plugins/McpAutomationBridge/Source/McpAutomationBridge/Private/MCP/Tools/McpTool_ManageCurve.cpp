// McpTool_ManageCurve.cpp — manage_curve tool definition (Ch5 UCurveFloat authoring)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ManageCurve : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_curve"); }

	FString GetDescription() const override
	{
		return TEXT("Create and edit UCurveFloat assets "
			"(keyframe editing with Auto/Linear/Constant/CubicBreak interp modes).");
	}

	FString GetCategory() const override { return TEXT("authoring"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("create_curve_float"),
				TEXT("set_curve_keys"),
				TEXT("get_curve_keys"),
				TEXT("inspect_curve")
			}, TEXT("Curve action to perform."))
			.String(TEXT("path"),
				TEXT("Package path (/Game/...). For create_curve_float, destination folder; else curve asset path."))
			.String(TEXT("name"), TEXT("Asset name (create_curve_float only)."))
			.ArrayOfObjects(TEXT("keys"),
				TEXT("Keyframes for set_curve_keys ({time, value, interpMode?})."),
				[](FMcpSchemaBuilder& S)
				{
					S.Number(TEXT("time"), TEXT("Key time."))
					 .Number(TEXT("value"), TEXT("Key value."))
					 .StringEnum(TEXT("interpMode"),
						{TEXT("Auto"), TEXT("Linear"), TEXT("Constant"), TEXT("CubicBreak")},
						TEXT("Interpolation mode. Auto=Cubic/Auto tangents. Default: Auto."))
					 .Required({TEXT("time"), TEXT("value")});
				})
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageCurve);
