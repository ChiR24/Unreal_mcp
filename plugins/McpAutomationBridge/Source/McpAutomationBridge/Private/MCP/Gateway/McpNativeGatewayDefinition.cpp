// McpNativeGatewayDefinition.cpp — static 'unreal' gateway tool definition

#include "MCP/Gateway/McpNativeGatewayDefinition.h"
#include "MCP/Registry/McpSchemaBuilder.h"
#include "MCP/Protocol/McpJsonRpc.h"

TSharedPtr<FJsonObject> BuildUnrealGatewayToolDefinition()
{
	auto InputSchema = FMcpSchemaBuilder()
		.StringEnum(TEXT("operation"),
			{ TEXT("search"), TEXT("describe"), TEXT("execute"), TEXT("configure") },
			TEXT("search finds capabilities by plain words. describe returns one capability's exact contract, "
				"or browses one level when given no selector. execute runs one validated action. "
				"configure enables or disables internal tool groups."))
		.String(TEXT("query"), TEXT("2-4 plain words naming the verb and the object, e.g. 'spawn actor', 'add variable blueprint', 'save level'. Matched against action names, topics, family, domain and summary; full sentences and filler words rank worse."))
		.String(TEXT("domain"), TEXT("Exact capability domain to filter search results by."))
		.String(TEXT("family"), TEXT("Exact capability family to filter search results by."))
		.String(TEXT("tool"), TEXT("Exact parent tool name copied from a search row (parentTool) or a describe response. Always paired with action; never guessed."))
		.String(TEXT("action"), TEXT("Exact action name copied from a search row or describe response. For configure, this is a manage_tools action."))
		.String(TEXT("param"), TEXT("Exact parameter name to inspect on one capability. Use with describe plus capability, or plus tool and action; returns that single parameter's schema."))
		.Object(TEXT("params"), TEXT("Parameters for execute or configure: an object whose keys are exactly the parameter names describe listed for this action, with the same casing. Never include action or subAction here."))
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
		// Read by McpNativeGatewayExecuteRequest.cpp; undeclared here, a schema-driven
		// client had no legal way to send an idempotency key or a timeout.
		.Object(TEXT("options"),
			TEXT("Execution controls for execute only; never put these inside params. Honored keys: "
				"idempotencyKey, expectedCatalogRevision, expectedRevisions, timeoutMs."),
			[](FMcpSchemaBuilder& Sub)
			{
				Sub.String(TEXT("idempotencyKey"),
					TEXT("Client-chosen key. A repeated execute with the same key and params returns the "
						"recorded receipt instead of running the action again."))
					.String(TEXT("expectedCatalogRevision"),
						TEXT("Refuse to run unless the catalog revision still equals this value; read it "
							"from any search or describe response."))
					.FreeformObject(TEXT("expectedRevisions"),
						TEXT("Live editor-state revisions to pin (selection, level, assetRegistry, package); "
							"the call is refused with STALE_STATE when one has moved."))
					.Integer(TEXT("timeoutMs"), TEXT("Deadline for this call in milliseconds (1-600000)."));
			})
		.Integer(TEXT("limit"), TEXT("Maximum rows per page: search results (default 12) or describe action rows (default 20). Defaults to 12."))
		.Integer(TEXT("offset"), TEXT("Zero-based offset into search results or into a described tool's action list. Defaults to 0."))
		.Integer(TEXT("maxBytes"), TEXT("Serialized byte ceiling for a search response. Results are dropped from the end until the response fits."))
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
			// Closed like consent: the honored option keys are enumerated above, and
			// the execute stage refuses any other key as UNSUPPORTED_OPTION anyway.
			const TSharedPtr<FJsonObject>* OptionsProp = nullptr;
			if (Props->TryGetObjectField(TEXT("options"), OptionsProp) && OptionsProp && (*OptionsProp).IsValid())
			{
				(*OptionsProp)->SetBoolField(TEXT("additionalProperties"), false);
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
			const TSharedPtr<FJsonObject>* MaxBytesProp = nullptr;
			if (Props->TryGetObjectField(TEXT("limit"), LimitProp) && LimitProp && (*LimitProp).IsValid())
			{
				(*LimitProp)->SetNumberField(TEXT("minimum"), 1);
				(*LimitProp)->SetNumberField(TEXT("maximum"), 25);
			}
			if (Props->TryGetObjectField(TEXT("offset"), OffsetProp) && OffsetProp && (*OffsetProp).IsValid())
			{
				(*OffsetProp)->SetNumberField(TEXT("minimum"), 0);
			}
			if (Props->TryGetObjectField(TEXT("maxBytes"), MaxBytesProp) && MaxBytesProp && (*MaxBytesProp).IsValid())
			{
				(*MaxBytesProp)->SetNumberField(TEXT("minimum"), 512);
				(*MaxBytesProp)->SetNumberField(TEXT("maximum"), 262144);
			}
		}
	}

	auto Tool = MakeShared<FJsonObject>();
	Tool->SetStringField(TEXT("name"), TEXT("unreal"));
	// Byte-identical to UNREAL_GATEWAY_DESCRIPTION in
	// src/tools/catalog/unreal-gateway-definition.ts; the unit test
	// unreal-gateway-guidance-contract.test.ts joins these pieces and diffs them.
	Tool->SetStringField(TEXT("description"),
		TEXT("Unreal Engine editor automation: one tool, four operations. Always in this order: "
			"(1) search with query = 2-4 plain words naming the verb and the object, e.g. 'spawn actor', 'add variable blueprint', 'save level'; every result row carries a one-line summary, its effect and a ready-made nextCall. "
			"(2) describe: send the chosen row's nextCall unchanged; the reply is the exact contract: parameters (name, type, required), inputSchema, a working example and, when required, consentGrant. "
			"(3) execute: send describe's nextCall with params filled in, using ONLY the parameter names describe listed. "
			"Rules: never guess capability ids, tool names, action names or parameter names; never put action or subAction inside params; a failed call returns suggestions and an executable nextCall, so send that nextCall instead of retrying blindly. "
			"To browse instead of search, call describe with no selector and follow the nextCall on each row one level down. "
			"configure only enables or disables internal tool groups; it never performs editor work."));
	Tool->SetStringField(TEXT("category"), TEXT("core"));
	Tool->SetObjectField(TEXT("inputSchema"), InputSchema);
	return Tool;
}
