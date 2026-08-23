#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AudioAuthoring/McpAutomationBridge_AudioAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpAudioAuthoring
{
static FString BuildMetaSoundClassName(const FString& ActualNamespace, const FString& ActualName, const FString& ActualVariant)
{
	return ActualNamespace.IsEmpty()
		? ActualName
		: (ActualVariant.IsEmpty()
			? FString::Printf(TEXT("%s.%s"), *ActualNamespace, *ActualName)
			: FString::Printf(TEXT("%s.%s.%s"), *ActualNamespace, *ActualName, *ActualVariant));
}

TSharedPtr<FJsonObject> HandleMetaSoundNodeActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
	if (SubAction == TEXT("add_metasound_node"))
	{
#if MCP_HAS_METASOUND && MCP_HAS_METASOUND_FRONTEND
		FString AssetPath = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("assetPath"), TEXT("")));
		FString NodeClassName = McpHandlerUtils::GetOptionalString(Params, TEXT("nodeClassName"), TEXT(""));
		FString NodeType = McpHandlerUtils::GetOptionalString(Params, TEXT("nodeType"), TEXT(""));
		bool bSave = McpHandlerUtils::GetOptionalBool(Params, TEXT("save"), true);

		if (AssetPath.IsEmpty())
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("MISSING_PATH"), TEXT("Asset path is required"));
		}

		UMetaSoundSource* MetaSound = Cast<UMetaSoundSource>(StaticLoadObject(UMetaSoundSource::StaticClass(), nullptr, *AssetPath));
		if (!MetaSound)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("ASSET_NOT_FOUND"), FString::Printf(TEXT("Could not load MetaSound: %s"), *AssetPath));
		}

		IMetaSoundDocumentInterface* DocInterface = Cast<IMetaSoundDocumentInterface>(MetaSound);
		if (!DocInterface)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("INTERFACE_ERROR"), TEXT("MetaSound does not implement document interface"));
		}

		TScriptInterface<IMetaSoundDocumentInterface> ScriptInterface(MetaSound);
#if MCP_HAS_METASOUND_FRONTEND_V2
		FMetaSoundFrontendDocumentBuilder Builder(ScriptInterface, nullptr, true);
#else
		FMetaSoundFrontendDocumentBuilder Builder(ScriptInterface);
#endif

		FString ActualNamespace;
		FString ActualName;
		FString ActualVariant;

		if (!NodeClassName.IsEmpty())
		{
			TArray<FString> Parts;
			NodeClassName.ParseIntoArray(Parts, TEXT("."));
			if (Parts.Num() == 3)
			{
				ActualNamespace = Parts[0];
				ActualName = Parts[1];
				ActualVariant = Parts[2];
			}
			else
			{
				ActualName = NodeClassName;
			}
		}
		else if (!NodeType.IsEmpty())
		{
			FString NodeTypeLower = NodeType.ToLower();
			if (NodeTypeLower == TEXT("oscillator") || NodeTypeLower == TEXT("sine")) { ActualNamespace = TEXT("UE"); ActualName = TEXT("Sine"); ActualVariant = TEXT("Audio"); }
			else if (NodeTypeLower == TEXT("gain") || NodeTypeLower == TEXT("multiply")) { ActualNamespace = TEXT("UE"); ActualName = TEXT("Multiply"); ActualVariant = TEXT("Float"); }
			else if (NodeTypeLower == TEXT("multiply_audio")) { ActualNamespace = TEXT("UE"); ActualName = TEXT("Multiply"); ActualVariant = TEXT("Audio"); }
			else if (NodeTypeLower == TEXT("add")) { ActualNamespace = TEXT("UE"); ActualName = TEXT("Add"); ActualVariant = TEXT("Float"); }
			else if (NodeTypeLower == TEXT("add_audio")) { ActualNamespace = TEXT("UE"); ActualName = TEXT("Add"); ActualVariant = TEXT("Audio"); }
			else if (NodeTypeLower == TEXT("waveplayer") || NodeTypeLower == TEXT("wave_player")) { ActualNamespace = TEXT("UE"); ActualName = TEXT("Wave Player"); ActualVariant = TEXT("Mono"); }
			else { ActualName = NodeType; }
		}

		if (ActualName.IsEmpty())
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("MISSING_NODE_TYPE"), TEXT("Node class name or type is required"));
		}

		FMetasoundFrontendClassName ClassName = FMetasoundFrontendClassName(FName(*ActualNamespace), FName(*ActualName), FName(*ActualVariant));
		const FMetasoundFrontendNode* NewNode = Builder.AddNodeByClassName(ClassName, 1, FGuid::NewGuid());
		FString FullClassName = BuildMetaSoundClassName(ActualNamespace, ActualName, ActualVariant);

		if (NewNode)
		{
			McpSafeAssetSave(MetaSound);
			Response->SetStringField(TEXT("nodeId"), NewNode->GetID().ToString());
			Response->SetStringField(TEXT("nodeClassName"), FullClassName);
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("message"), FString::Printf(TEXT("MetaSound node '%s' added"), *FullClassName));
			McpHandlerUtils::AddVerification(Response, MetaSound);
		}
		else
		{
			Response->SetBoolField(TEXT("success"), false);
			Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Node class '%s' not found in MetaSound registry"), *FullClassName));
			Response->SetStringField(TEXT("errorCode"), TEXT("NODE_CLASS_NOT_FOUND"));
			Response->SetStringField(TEXT("code"), TEXT("NODE_CLASS_NOT_FOUND"));
			TArray<FString> Accepted = { TEXT("oscillator/sine -> UE.Sine.Audio"), TEXT("gain/multiply -> UE.Multiply.Float"), TEXT("multiply_audio -> UE.Multiply.Audio"), TEXT("add -> UE.Add.Float"), TEXT("add_audio -> UE.Add.Audio"), TEXT("waveplayer/wave_player -> UE.Wave Player.Mono") };
			TArray<TSharedPtr<FJsonValue>> AcceptedArray;
			for (const FString& A : Accepted) { AcceptedArray.Add(MakeShared<FJsonValueString>(A)); }
			Response->SetArrayField(TEXT("acceptedNodeTypes"), AcceptedArray);
		}

#if MCP_HAS_METASOUND_FRONTEND_V2
		Builder.FinishBuilding();
#endif
		return Response;
#elif MCP_HAS_METASOUND
		FString AssetPath = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("assetPath"), TEXT("")));
		FString NodeType = McpHandlerUtils::GetOptionalString(Params, TEXT("nodeType"), TEXT(""));
		Response->SetBoolField(TEXT("success"), false);
		Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Cannot add MetaSound node '%s' - Frontend Builder not available"), *NodeType));
		Response->SetStringField(TEXT("errorCode"), TEXT("METASOUND_FRONTEND_NOT_SUPPORTED"));
		Response->SetStringField(TEXT("code"), TEXT("METASOUND_FRONTEND_NOT_SUPPORTED"));
		Response->SetStringField(TEXT("requiredVersion"), TEXT("UE 5.3+"));
		return Response;
#else
		return McpHandlerUtils::BuildErrorResponse(TEXT("METASOUND_NOT_AVAILABLE"), TEXT("MetaSound support not available"));
#endif
	}

	if (SubAction == TEXT("connect_metasound_nodes"))
	{
		return HandleMetaSoundNodeConnect(Params, Response);
	}

	return nullptr;
}
}
#endif