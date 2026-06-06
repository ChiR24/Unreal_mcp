#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AudioAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpAudioAuthoring
{
TSharedPtr<FJsonObject> HandleMetaSoundInterfaceActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
	if (SubAction == TEXT("add_metasound_input"))
	{
#if MCP_HAS_METASOUND && MCP_HAS_METASOUND_FRONTEND
		FString AssetPath = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("assetPath"), TEXT("")));
		FString InputName = McpHandlerUtils::GetOptionalString(Params, TEXT("inputName"), TEXT(""));
		FString InputType = McpHandlerUtils::GetOptionalString(Params, TEXT("inputType"), TEXT("Float"));
		bool bSave = McpHandlerUtils::GetOptionalBool(Params, TEXT("save"), true);

		if (AssetPath.IsEmpty()) { return McpHandlerUtils::BuildErrorResponse(TEXT("MISSING_PATH"), TEXT("Asset path is required")); }
		if (InputName.IsEmpty()) { return McpHandlerUtils::BuildErrorResponse(TEXT("MISSING_INPUT_NAME"), TEXT("Input name is required")); }

		UMetaSoundSource* MetaSound = Cast<UMetaSoundSource>(StaticLoadObject(UMetaSoundSource::StaticClass(), nullptr, *AssetPath));
		if (!MetaSound)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("ASSET_NOT_FOUND"), FString::Printf(TEXT("Could not load MetaSound: %s"), *AssetPath));
		}

		TScriptInterface<IMetaSoundDocumentInterface> ScriptInterface(MetaSound);
#if MCP_HAS_METASOUND_FRONTEND_V2
		FMetaSoundFrontendDocumentBuilder Builder(ScriptInterface, nullptr, true);
#else
		FMetaSoundFrontendDocumentBuilder Builder(ScriptInterface);
#endif

		FMetasoundFrontendClassInput ClassInput;
		ClassInput.Name = FName(*InputName);
		ClassInput.TypeName = FName(*InputType);
		ClassInput.VertexID = FGuid::NewGuid();
		ClassInput.NodeID = FGuid::NewGuid();
		ClassInput.AccessType = EMetasoundFrontendVertexAccessType::Reference;
		const FMetasoundFrontendNode* InputNode = Builder.AddGraphInput(ClassInput);

		if (InputNode)
		{
			McpSafeAssetSave(MetaSound);
			Response->SetStringField(TEXT("inputName"), InputName);
			Response->SetStringField(TEXT("inputType"), InputType);
			Response->SetStringField(TEXT("nodeId"), InputNode->GetID().ToString());
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("message"), FString::Printf(TEXT("MetaSound input '%s' added"), *InputName));
			McpHandlerUtils::AddVerification(Response, MetaSound);
		}
		else
		{
			Response->SetBoolField(TEXT("success"), false);
			Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to add input '%s' - type '%s' may not be valid"), *InputName, *InputType));
			Response->SetStringField(TEXT("errorCode"), TEXT("INPUT_FAILED"));
			Response->SetStringField(TEXT("code"), TEXT("INPUT_FAILED"));
		}

#if MCP_HAS_METASOUND_FRONTEND_V2
		Builder.FinishBuilding();
#endif
		return Response;
#elif MCP_HAS_METASOUND
		FString AssetPath = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("assetPath"), TEXT("")));
		FString InputName = McpHandlerUtils::GetOptionalString(Params, TEXT("inputName"), TEXT(""));
		FString InputType = McpHandlerUtils::GetOptionalString(Params, TEXT("inputType"), TEXT("Float"));
		Response->SetStringField(TEXT("inputName"), InputName);
		Response->SetStringField(TEXT("inputType"), InputType);
		Response->SetBoolField(TEXT("success"), true);
		Response->SetStringField(TEXT("message"), FString::Printf(TEXT("MetaSound input '%s' noted"), *InputName));
		Response->SetStringField(TEXT("note"), TEXT("MetaSound Frontend Builder not available - upgrade to UE 5.3+ for full support"));
		return Response;
#else
		return McpHandlerUtils::BuildErrorResponse(TEXT("METASOUND_NOT_AVAILABLE"), TEXT("MetaSound support not available"));
#endif
	}

	if (SubAction == TEXT("add_metasound_output"))
	{
#if MCP_HAS_METASOUND && MCP_HAS_METASOUND_FRONTEND
		FString AssetPath = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("assetPath"), TEXT("")));
		FString OutputName = McpHandlerUtils::GetOptionalString(Params, TEXT("outputName"), TEXT(""));
		FString OutputType = McpHandlerUtils::GetOptionalString(Params, TEXT("outputType"), TEXT("Audio"));
		bool bSave = McpHandlerUtils::GetOptionalBool(Params, TEXT("save"), true);

		if (AssetPath.IsEmpty()) { return McpHandlerUtils::BuildErrorResponse(TEXT("MISSING_PATH"), TEXT("Asset path is required")); }
		if (OutputName.IsEmpty()) { return McpHandlerUtils::BuildErrorResponse(TEXT("MISSING_OUTPUT_NAME"), TEXT("Output name is required")); }

		UMetaSoundSource* MetaSound = Cast<UMetaSoundSource>(StaticLoadObject(UMetaSoundSource::StaticClass(), nullptr, *AssetPath));
		if (!MetaSound)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("ASSET_NOT_FOUND"), FString::Printf(TEXT("Could not load MetaSound: %s"), *AssetPath));
		}

		TScriptInterface<IMetaSoundDocumentInterface> ScriptInterface(MetaSound);
#if MCP_HAS_METASOUND_FRONTEND_V2
		FMetaSoundFrontendDocumentBuilder Builder(ScriptInterface, nullptr, true);
#else
		FMetaSoundFrontendDocumentBuilder Builder(ScriptInterface);
#endif

		FMetasoundFrontendClassOutput ClassOutput;
		ClassOutput.Name = FName(*OutputName);
		ClassOutput.TypeName = FName(*OutputType);
		ClassOutput.VertexID = FGuid::NewGuid();
		ClassOutput.NodeID = FGuid::NewGuid();
		ClassOutput.AccessType = EMetasoundFrontendVertexAccessType::Reference;
		const FMetasoundFrontendNode* OutputNode = Builder.AddGraphOutput(ClassOutput);

		if (OutputNode)
		{
			McpSafeAssetSave(MetaSound);
			Response->SetStringField(TEXT("outputName"), OutputName);
			Response->SetStringField(TEXT("outputType"), OutputType);
			Response->SetStringField(TEXT("nodeId"), OutputNode->GetID().ToString());
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("message"), FString::Printf(TEXT("MetaSound output '%s' added"), *OutputName));
			McpHandlerUtils::AddVerification(Response, MetaSound);
		}
		else
		{
			Response->SetBoolField(TEXT("success"), false);
			Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to add output '%s' - type '%s' may not be valid"), *OutputName, *OutputType));
			Response->SetStringField(TEXT("errorCode"), TEXT("OUTPUT_FAILED"));
			Response->SetStringField(TEXT("code"), TEXT("OUTPUT_FAILED"));
		}

#if MCP_HAS_METASOUND_FRONTEND_V2
		Builder.FinishBuilding();
#endif
		return Response;
#elif MCP_HAS_METASOUND
		FString AssetPath = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("assetPath"), TEXT("")));
		FString OutputName = McpHandlerUtils::GetOptionalString(Params, TEXT("outputName"), TEXT(""));
		FString OutputType = McpHandlerUtils::GetOptionalString(Params, TEXT("outputType"), TEXT("Audio"));
		Response->SetStringField(TEXT("outputName"), OutputName);
		Response->SetStringField(TEXT("outputType"), OutputType);
		Response->SetBoolField(TEXT("success"), true);
		Response->SetStringField(TEXT("message"), FString::Printf(TEXT("MetaSound output '%s' noted"), *OutputName));
		Response->SetStringField(TEXT("note"), TEXT("MetaSound Frontend Builder not available - upgrade to UE 5.3+ for full support"));
		return Response;
#else
		return McpHandlerUtils::BuildErrorResponse(TEXT("METASOUND_NOT_AVAILABLE"), TEXT("MetaSound support not available"));
#endif
	}

	if (SubAction == TEXT("set_metasound_default"))
	{
#if MCP_HAS_METASOUND && MCP_HAS_METASOUND_FRONTEND
		FString AssetPath = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("assetPath"), TEXT("")));
		FString InputName = McpHandlerUtils::GetOptionalString(Params, TEXT("inputName"), TEXT(""));
		bool bSave = McpHandlerUtils::GetOptionalBool(Params, TEXT("save"), true);

		if (AssetPath.IsEmpty()) { return McpHandlerUtils::BuildErrorResponse(TEXT("MISSING_PATH"), TEXT("Asset path is required")); }
		if (InputName.IsEmpty()) { return McpHandlerUtils::BuildErrorResponse(TEXT("MISSING_INPUT_NAME"), TEXT("Input name is required")); }

		UMetaSoundSource* MetaSound = Cast<UMetaSoundSource>(StaticLoadObject(UMetaSoundSource::StaticClass(), nullptr, *AssetPath));
		if (!MetaSound)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("ASSET_NOT_FOUND"), FString::Printf(TEXT("Could not load MetaSound: %s"), *AssetPath));
		}

		TScriptInterface<IMetaSoundDocumentInterface> ScriptInterface(MetaSound);
#if MCP_HAS_METASOUND_FRONTEND_V2
		FMetaSoundFrontendDocumentBuilder Builder(ScriptInterface, nullptr, true);
#else
		FMetaSoundFrontendDocumentBuilder Builder(ScriptInterface);
#endif

		FMetasoundFrontendLiteral Literal;
		if (Params->HasField(TEXT("floatValue"))) { Literal.Set(static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("floatValue"), 0.0))); }
		else if (Params->HasField(TEXT("intValue"))) { Literal.Set(static_cast<int32>(McpHandlerUtils::GetOptionalInt(Params, TEXT("intValue"), 0))); }
		else if (Params->HasField(TEXT("boolValue"))) { Literal.Set(McpHandlerUtils::GetOptionalBool(Params, TEXT("boolValue"), false)); }
		else if (Params->HasField(TEXT("stringValue"))) { Literal.Set(McpHandlerUtils::GetOptionalString(Params, TEXT("stringValue"), TEXT(""))); }
		else { Literal.Set(0.0f); }

		bool bSuccess = Builder.SetGraphInputDefault(FName(*InputName), Literal);
		if (bSuccess)
		{
			McpSafeAssetSave(MetaSound);
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("message"), FString::Printf(TEXT("MetaSound default for '%s' set"), *InputName));
			McpHandlerUtils::AddVerification(Response, MetaSound);
		}
		else
		{
			Response->SetBoolField(TEXT("success"), false);
			Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to set default for input '%s'"), *InputName));
			Response->SetStringField(TEXT("errorCode"), TEXT("SET_DEFAULT_FAILED"));
			Response->SetStringField(TEXT("code"), TEXT("SET_DEFAULT_FAILED"));
		}

#if MCP_HAS_METASOUND_FRONTEND_V2
		Builder.FinishBuilding();
#endif
		return Response;
#elif MCP_HAS_METASOUND
		FString AssetPath = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("assetPath"), TEXT("")));
		FString InputName = McpHandlerUtils::GetOptionalString(Params, TEXT("inputName"), TEXT(""));
		Response->SetBoolField(TEXT("success"), true);
		Response->SetStringField(TEXT("message"), FString::Printf(TEXT("MetaSound default for '%s' noted"), *InputName));
		Response->SetStringField(TEXT("note"), TEXT("MetaSound Frontend Builder not available - upgrade to UE 5.3+ for full support"));
		return Response;
#else
		return McpHandlerUtils::BuildErrorResponse(TEXT("METASOUND_NOT_AVAILABLE"), TEXT("MetaSound support not available"));
#endif
	}

	return nullptr;
}
}
#endif
