// McpNativeGatewayDefinition.cpp — static 'unreal' gateway tool definition

#include "MCP/Gateway/McpNativeGatewayDefinition.h"
#include "MCP/Registry/McpSchemaBuilder.h"
#include "MCP/Protocol/McpJsonRpc.h"

TSharedPtr<FJsonObject> BuildUnrealGatewayToolDefinition()
{
	auto InputSchema = FMcpSchemaBuilder()
		.StringEnum(TEXT("operation"),
			{ TEXT("search"), TEXT("describe"), TEXT("execute"), TEXT("configure") },
			TEXT("search finds capabilities. describe returns an exact parent-tool contract. "
				"execute runs one validated action. configure manages internal capability availability."))
		.String(TEXT("query"), TEXT("Search words matched against capability id, family, domain, topics, and summary."))
		.String(TEXT("domain"), TEXT("Exact capability domain to browse. Use with search only."))
		.String(TEXT("family"), TEXT("Exact capability family to browse. Use with search only."))
		.String(TEXT("tool"), TEXT("Exact canonical parent tool name returned by search or describe."))
		.String(TEXT("action"), TEXT("Exact action name returned by describe. For configure, this is a manage_tools action."))
		.String(TEXT("param"), TEXT("Exact parameter name (tool-union catalog) to inspect. Requires tool and action for full drill-down; resolves the single parameter schema. Use with describe only."))
		.Object(TEXT("params"), TEXT("Parameters for execute or configure. Never include action or subAction here."))
		.Object(TEXT("consent"),
			TEXT("Per-call consent grant for a capability whose policy.consent is not 'none'. Bound to one "
				"capability and one call; never persisted, inherited or reused. Read the exact grant from "
				"describe.consentGrant. Use with execute only."),
			[](FMcpSchemaBuilder& Sub)
			{
				Sub.String(TEXT("capability"),
					TEXT("Exact canonical capability ID this grant authorizes, as returned by describe."))
					.StringEnum(TEXT("acknowledge"), { TEXT("explicit"), TEXT("elevated") },
						TEXT("Acknowledgement strength. 'explicit' satisfies an explicit policy; 'elevated' "
							"is required by a destructive policy and also satisfies explicit."))
					.Required({ TEXT("capability"), TEXT("acknowledge") });
			})
		.Integer(TEXT("limit"), TEXT("Maximum search results to return. Defaults to 12."))
		.Integer(TEXT("offset"), TEXT("Zero-based search result offset. Defaults to 0."))
		.Required({ TEXT("operation") })
		.Build();

	// Mirror the TypeScript gateway contract: the gateway level rejects extra properties.
	InputSchema->SetBoolField(TEXT("additionalProperties"), false);

	// Match the TS gateway schema bounds (parity audit only covers canonical 23).
	if (InputSchema->HasField(TEXT("properties")))
	{
		const TSharedPtr<FJsonObject> Props = InputSchema->GetObjectField(TEXT("properties"));
		if (Props.IsValid())
		{
			const TSharedPtr<FJsonObject>* ConsentProp = nullptr;
			if (Props->TryGetObjectField(TEXT("consent"), ConsentProp) && ConsentProp && (*ConsentProp).IsValid())
			{
				(*ConsentProp)->SetBoolField(TEXT("additionalProperties"), false);
			}
			// Open on purpose, unlike consent above: params keys are per-action and
			// unenumerable here, so closing it would reject every execute.
			const TSharedPtr<FJsonObject>* ParamsProp = nullptr;
			if (Props->TryGetObjectField(TEXT("params"), ParamsProp) && ParamsProp && (*ParamsProp).IsValid())
			{
				(*ParamsProp)->SetBoolField(TEXT("additionalProperties"), true);
			}
			const TSharedPtr<FJsonObject>* LimitProp = nullptr;
			const TSharedPtr<FJsonObject>* OffsetProp = nullptr;
			if (Props->TryGetObjectField(TEXT("limit"), LimitProp) && LimitProp && (*LimitProp).IsValid())
			{
				(*LimitProp)->SetNumberField(TEXT("minimum"), 1);
				(*LimitProp)->SetNumberField(TEXT("maximum"), 25);
			}
			if (Props->TryGetObjectField(TEXT("offset"), OffsetProp) && OffsetProp && (*OffsetProp).IsValid())
			{
				(*OffsetProp)->SetNumberField(TEXT("minimum"), 0);
			}
		}
	}

	auto Tool = MakeShared<FJsonObject>();
	Tool->SetStringField(TEXT("name"), TEXT("unreal"));
	Tool->SetStringField(TEXT("description"),
		TEXT("Unreal Engine capability gateway. Search first, describe the exact contract, then execute a "
			"validated action. Use configure only to manage internal capability availability."));
	Tool->SetStringField(TEXT("category"), TEXT("core"));
	Tool->SetObjectField(TEXT("inputSchema"), InputSchema);
	return Tool;
}
