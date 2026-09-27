#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AudioAuthoring/McpAutomationBridge_AudioAuthoringHandlersPrivate.h"

namespace McpAudioAuthoring
{
// add_metasound_input / add_metasound_output: one graph vertex of that direction.
static TSharedPtr<FJsonObject> AddMetaSoundGraphVertex(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response, bool bInput)
{
#if MCP_HAS_METASOUND && MCP_HAS_METASOUND_FRONTEND
	const FString Kind = bInput ? TEXT("input") : TEXT("output");
	const FString AssetPath = NormalizeAudioPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
	const FString VertexName = GetJsonStringField(Params, *(Kind + TEXT("Name")), TEXT(""));
	const FString VertexType = GetJsonStringField(Params, *(Kind + TEXT("Type")), bInput ? TEXT("Float") : TEXT("Audio"));
	const bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

	if (AssetPath.IsEmpty()) { return McpHandlerUtils::BuildErrorResponse(TEXT("MISSING_PATH"), TEXT("Asset path is required")); }
	if (VertexName.IsEmpty())
	{
		return McpHandlerUtils::BuildErrorResponse(bInput ? TEXT("MISSING_INPUT_NAME") : TEXT("MISSING_OUTPUT_NAME"), FString::Printf(TEXT("%sName is required"), *Kind));
	}

	UMetaSoundSource* MetaSound = Cast<UMetaSoundSource>(StaticLoadObject(UMetaSoundSource::StaticClass(), nullptr, *AssetPath));
	if (!MetaSound)
	{
		return McpHandlerUtils::BuildErrorResponse(TEXT("ASSET_NOT_FOUND"), FString::Printf(TEXT("Could not load MetaSound: %s"), *AssetPath));
	}

	TScriptInterface<IMetaSoundDocumentInterface> ScriptInterface(MetaSound);
	MCP_METASOUND_BUILDER(Builder, ScriptInterface);

	auto Describe = [&](FMetasoundFrontendClassVertex& Vertex)
	{
		Vertex.Name = FName(*VertexName);
		Vertex.TypeName = FName(*VertexType);
		Vertex.VertexID = FGuid::NewGuid();
		Vertex.NodeID = FGuid::NewGuid();
		Vertex.AccessType = EMetasoundFrontendVertexAccessType::Reference;
	};
	const FMetasoundFrontendNode* Node = nullptr;
	if (bInput)
	{
		FMetasoundFrontendClassInput ClassInput;
		Describe(ClassInput);
		Node = Builder.AddGraphInput(ClassInput);
		// The contract declares defaultValue for inputs; it used to be dropped.
		if (Node && Params->HasField(TEXT("defaultValue")))
		{
			FMetasoundFrontendLiteral Literal;
			FString LiteralError;
			const bool bDefaultSet = MetaSoundLiteralFromParams(Params, VertexType, Literal, LiteralError) &&
				Builder.SetGraphInputDefault(ClassInput.Name, Literal);
			Response->SetBoolField(TEXT("defaultSet"), bDefaultSet);
			if (!bDefaultSet) { Response->SetStringField(TEXT("defaultError"), LiteralError.IsEmpty() ? TEXT("value does not fit the input type") : LiteralError); }
		}
	}
	else
	{
		FMetasoundFrontendClassOutput ClassOutput;
		Describe(ClassOutput);
		Node = Builder.AddGraphOutput(ClassOutput);
	}

	if (Node)
	{
		if (bSave) { McpSafeAssetSave(MetaSound); }
		Response->SetStringField(Kind + TEXT("Name"), VertexName);
		Response->SetStringField(Kind + TEXT("Type"), VertexType);
		Response->SetStringField(TEXT("nodeId"), Node->GetID().ToString());
		Response->SetBoolField(TEXT("success"), true);
		Response->SetStringField(TEXT("message"), FString::Printf(TEXT("MetaSound %s '%s' added"), *Kind, *VertexName));
		McpHandlerUtils::AddVerification(Response, MetaSound);
	}
	else
	{
		Response->SetBoolField(TEXT("success"), false);
		Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to add %s '%s' - type '%s' may not be valid"), *Kind, *VertexName, *VertexType));
		Response->SetStringField(TEXT("code"), bInput ? TEXT("INPUT_FAILED") : TEXT("OUTPUT_FAILED"));
	}

	MCP_METASOUND_FINISH(Builder);
	return Response;
#else
	// Without the Frontend document builder (UE 5.3+) nothing can be added; this used to report success.
	return McpHandlerUtils::BuildErrorResponse(TEXT("METASOUND_NOT_AVAILABLE"), TEXT("Adding MetaSound graph inputs or outputs requires the MetaSound Frontend builder (UE 5.3+)"));
#endif
}

TSharedPtr<FJsonObject> HandleMetaSoundInterfaceActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
	if (SubAction == TEXT("add_metasound_input") || SubAction == TEXT("add_metasound_output"))
	{
		return AddMetaSoundGraphVertex(Params, Response, SubAction == TEXT("add_metasound_input"));
	}

	if (SubAction == TEXT("set_metasound_default"))
	{
		return HandleMetaSoundDefaultAction(Params, Response);
	}

	return nullptr;
}
}
