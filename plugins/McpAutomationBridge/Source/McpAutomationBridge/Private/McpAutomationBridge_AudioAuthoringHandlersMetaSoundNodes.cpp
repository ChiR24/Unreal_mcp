#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AudioAuthoringHandlersPrivate.h"

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
#if MCP_HAS_METASOUND && MCP_HAS_METASOUND_FRONTEND
		FString AssetPath = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("assetPath"), TEXT("")));
		FString SourceNodeId = McpHandlerUtils::GetOptionalString(Params, TEXT("sourceNodeId"), TEXT(""));
		FString SourceOutputName = McpHandlerUtils::GetOptionalString(Params, TEXT("sourceOutputName"), TEXT(""));
		FString TargetNodeId = McpHandlerUtils::GetOptionalString(Params, TEXT("targetNodeId"), TEXT(""));
		FString TargetInputName = McpHandlerUtils::GetOptionalString(Params, TEXT("targetInputName"), TEXT(""));
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

		TScriptInterface<IMetaSoundDocumentInterface> ScriptInterface(MetaSound);
#if MCP_HAS_METASOUND_FRONTEND_V2
		FMetaSoundFrontendDocumentBuilder Builder(ScriptInterface, nullptr, true);
#else
		FMetaSoundFrontendDocumentBuilder Builder(ScriptInterface);
#endif

		FGuid SourceGuid;
		FGuid TargetGuid;
		if (!FGuid::Parse(SourceNodeId, SourceGuid) || !FGuid::Parse(TargetNodeId, TargetGuid))
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("INVALID_GUID"), TEXT("Invalid node ID format - must be valid GUID"));
		}

		Metasound::Frontend::FNamedEdge NamedEdge{SourceGuid, FName(*SourceOutputName), TargetGuid, FName(*TargetInputName)};
		TSet<Metasound::Frontend::FNamedEdge> Edges;
		Edges.Add(NamedEdge);
		TArray<const FMetasoundFrontendEdge*> CreatedEdges;
		bool bSuccess = Builder.AddNamedEdges(Edges, &CreatedEdges, true);

		if (bSuccess && CreatedEdges.Num() > 0)
		{
			McpSafeAssetSave(MetaSound);
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("message"), TEXT("MetaSound nodes connected"));
			Response->SetNumberField(TEXT("edgesCreated"), CreatedEdges.Num());
			McpHandlerUtils::AddVerification(Response, MetaSound);
		}
		else
		{
			Response->SetBoolField(TEXT("success"), false);
			Response->SetStringField(TEXT("error"), TEXT("Failed to create edge connection"));
			Response->SetStringField(TEXT("errorCode"), TEXT("EDGE_FAILED"));
			Response->SetStringField(TEXT("code"), TEXT("EDGE_FAILED"));
			TArray<TSharedPtr<FJsonValue>> NodeIdArray;
#if MCP_HAS_METASOUND_FRONTEND_V2
			const FMetasoundFrontendDocument& Doc = Builder.GetConstDocumentChecked();
			Doc.RootGraph.IterateGraphPages([&NodeIdArray](const FMetasoundFrontendGraph& GraphPage)
			{
				for (const FMetasoundFrontendNode& Node : GraphPage.Nodes)
				{
					TSharedPtr<FJsonObject> NodeObj = McpHandlerUtils::CreateResultObject();
					NodeObj->SetStringField(TEXT("nodeId"), Node.GetID().ToString());
					NodeObj->SetStringField(TEXT("name"), Node.Name.ToString());
					NodeIdArray.Add(MakeShared<FJsonValueObject>(NodeObj));
				}
			});
#endif
			if (NodeIdArray.Num() > 0)
			{
				Response->SetArrayField(TEXT("availableNodes"), NodeIdArray);
			}
		}

#if MCP_HAS_METASOUND_FRONTEND_V2
		Builder.FinishBuilding();
#endif
		return Response;
#elif MCP_HAS_METASOUND
		FString AssetPath = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("assetPath"), TEXT("")));
		Response->SetBoolField(TEXT("success"), false);
		Response->SetStringField(TEXT("error"), TEXT("Cannot connect MetaSound nodes - Frontend Builder not available"));
		Response->SetStringField(TEXT("errorCode"), TEXT("METASOUND_FRONTEND_NOT_SUPPORTED"));
		Response->SetStringField(TEXT("code"), TEXT("METASOUND_FRONTEND_NOT_SUPPORTED"));
		Response->SetStringField(TEXT("requiredVersion"), TEXT("UE 5.3+"));
		return Response;
#else
		return McpHandlerUtils::BuildErrorResponse(TEXT("METASOUND_NOT_AVAILABLE"), TEXT("MetaSound support not available"));
#endif
	}

	return nullptr;
}
}
#endif
