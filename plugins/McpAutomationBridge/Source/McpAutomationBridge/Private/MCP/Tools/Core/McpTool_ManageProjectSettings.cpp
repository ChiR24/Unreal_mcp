// McpTool_ManageProjectSettings.cpp — manage_project_settings tool definition

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "MCP/Registry/McpToolDefinition.h"
#include "MCP/Registry/McpToolRegistry.h"
#include "MCP/Registry/McpSchemaBuilder.h"

class FMcpTool_ManageProjectSettings : public FMcpToolDefinition
{
public:
    FString GetName() const override { return TEXT("manage_project_settings"); }
    FString GetDescription() const override { return TEXT("Manage Unreal Engine project settings, including collision profiles, channels, object types, and physical materials."); }
    FString GetCategory() const override { return TEXT("core"); }

    TSharedPtr<FJsonObject> BuildInputSchema() const override
    {
        return FMcpSchemaBuilder()
            .StringEnum(TEXT("action"), {
                TEXT("create_collision_channel"),
                TEXT("create_collision_profile"),
                TEXT("configure_channel_responses"),
                TEXT("configure_object_type"),
                TEXT("configure_trace_channel"),
                TEXT("set_actor_collision_profile"),
                TEXT("create_physical_material"),
                TEXT("set_physical_material_properties")
            }, TEXT("Project settings action"))
            .Required({TEXT("action")})
            .Build();
    }
};

MCP_REGISTER_TOOL(FMcpTool_ManageProjectSettings);
