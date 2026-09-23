// MetaSound node graph connect actions for connect_metasound_nodes (split from
// McpAutomationBridge_AudioAuthoringHandlersMetaSoundNodes.cpp).
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AudioAuthoring/McpAutomationBridge_AudioAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpAudioAuthoring
{
namespace
{
// The editor shows the Source interface members as "On Play", "On Finished" and
// "Out Mono"; the document names those nodes (and their single pin) by vertex
// name, so the names every caller reaches for matched nothing.
FString MetaSoundInterfaceVertexName(const FString& Ref)
{
	const FString Key = Ref.Replace(TEXT(" "), TEXT(""));
	if (Key.Equals(TEXT("OnPlay"), ESearchCase::IgnoreCase)) { return TEXT("UE.Source.OnPlay"); }
	if (Key.Equals(TEXT("OnFinished"), ESearchCase::IgnoreCase)) { return TEXT("UE.Source.OneShot.OnFinished"); }
	if (Key.Equals(TEXT("OutMono"), ESearchCase::IgnoreCase)) { return TEXT("UE.OutputFormat.Mono.Audio:0"); }
	if (Key.Equals(TEXT("OutLeft"), ESearchCase::IgnoreCase)) { return TEXT("UE.OutputFormat.Stereo.Audio:0"); }
	if (Key.Equals(TEXT("OutRight"), ESearchCase::IgnoreCase)) { return TEXT("UE.OutputFormat.Stereo.Audio:1"); }
	return Ref;
}

// Only an interface node's own pin is renamed: ordinary nodes (Wave Player)
// carry a real "On Finished" pin of their own.
void AliasInterfaceEndpoint(FString& NodeRef, FString& PinName)
{
	NodeRef = MetaSoundInterfaceVertexName(NodeRef);
	if ((NodeRef.StartsWith(TEXT("UE.Source.")) || NodeRef.StartsWith(TEXT("UE.OutputFormat."))) &&
		(PinName.IsEmpty() || MetaSoundInterfaceVertexName(PinName) == NodeRef))
	{
		PinName = NodeRef;
	}
}
}

TSharedPtr<FJsonObject> HandleMetaSoundNodeConnect(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
#if MCP_HAS_METASOUND && MCP_HAS_METASOUND_FRONTEND
		FString AssetPath = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("assetPath"), TEXT("")));
		FString SourceNodeId = McpHandlerUtils::GetOptionalString(Params, TEXT("sourceNodeId"), TEXT(""));
		FString SourceOutputName = McpHandlerUtils::GetOptionalString(Params, TEXT("sourceOutputName"), TEXT(""));
		FString TargetNodeId = McpHandlerUtils::GetOptionalString(Params, TEXT("targetNodeId"), TEXT(""));
		FString TargetInputName = McpHandlerUtils::GetOptionalString(Params, TEXT("targetInputName"), TEXT(""));
		AliasInterfaceEndpoint(SourceNodeId, SourceOutputName);
		AliasInterfaceEndpoint(TargetNodeId, TargetInputName);
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
#if MCP_HAS_METASOUND_FRONTEND_V2
		// Snapshot the graph's nodes once: used to resolve node references given
		// by name instead of GUID, and to enrich failure responses so the caller
		// can self-correct (callers commonly pass node names or wrong pin names).
		struct FMcpNodeSnapshot
		{
			FGuid Id;
			FString Name;
			FString ClassName;
			TArray<FString> Inputs;
			TArray<FString> Outputs;
		};
		TArray<FMcpNodeSnapshot> GraphNodes;
		{
			const FMetasoundFrontendDocument& Doc = Builder.GetConstDocumentChecked();
			Doc.RootGraph.IterateGraphPages([&GraphNodes, &Doc](const FMetasoundFrontendGraph& GraphPage)
			{
				for (const FMetasoundFrontendNode& Node : GraphPage.Nodes)
				{
					FMcpNodeSnapshot Snapshot;
					Snapshot.Id = Node.GetID();
					Snapshot.Name = Node.Name.ToString();
					if (const FMetasoundFrontendClass* NodeClass = Doc.Dependencies.FindByPredicate(
						[&Node](const FMetasoundFrontendClass& Dependency) { return Dependency.ID == Node.ClassID; }))
					{
						Snapshot.ClassName = NodeClass->Metadata.GetClassName().ToString();
					}
					for (const FMetasoundFrontendVertex& Input : Node.Interface.Inputs)
					{
						Snapshot.Inputs.Add(FString::Printf(TEXT("%s (%s)"), *Input.Name.ToString(), *Input.TypeName.ToString()));
					}
					for (const FMetasoundFrontendVertex& Output : Node.Interface.Outputs)
					{
						Snapshot.Outputs.Add(FString::Printf(TEXT("%s (%s)"), *Output.Name.ToString(), *Output.TypeName.ToString()));
					}
					GraphNodes.Add(MoveTemp(Snapshot));
				}
			});
		}

		auto BuildAvailableNodesArray = [&GraphNodes]()
		{
			TArray<TSharedPtr<FJsonValue>> NodeArray;
			for (const FMcpNodeSnapshot& Snapshot : GraphNodes)
			{
				TSharedPtr<FJsonObject> NodeObj = McpHandlerUtils::CreateResultObject();
				NodeObj->SetStringField(TEXT("nodeId"), Snapshot.Id.ToString());
				NodeObj->SetStringField(TEXT("name"), Snapshot.Name);
				if (!Snapshot.ClassName.IsEmpty())
				{
					NodeObj->SetStringField(TEXT("className"), Snapshot.ClassName);
				}
				TArray<TSharedPtr<FJsonValue>> InputArray;
				for (const FString& Input : Snapshot.Inputs) { InputArray.Add(MakeShared<FJsonValueString>(Input)); }
				NodeObj->SetArrayField(TEXT("inputs"), InputArray);
				TArray<TSharedPtr<FJsonValue>> OutputArray;
				for (const FString& Output : Snapshot.Outputs) { OutputArray.Add(MakeShared<FJsonValueString>(Output)); }
				NodeObj->SetArrayField(TEXT("outputs"), OutputArray);
				NodeArray.Add(MakeShared<FJsonValueObject>(NodeObj));
			}
			return NodeArray;
		};

		// Resolve a node reference: GUID first, then a unique (case-insensitive)
		// match on node name or full class name.
		auto ResolveNodeReference = [&GraphNodes](const FString& NodeRef, FGuid& OutGuid, FString& OutFailReason) -> bool
		{
			// A well-formed GUID must still name a node that exists in this graph;
			// otherwise fall through to the INVALID_GUID + availableNodes diagnostic
			// instead of failing later with a generic edge error.
			FGuid ParsedGuid;
			if (FGuid::Parse(NodeRef, ParsedGuid))
			{
				if (GraphNodes.ContainsByPredicate([&ParsedGuid](const FMcpNodeSnapshot& Snapshot) { return Snapshot.Id == ParsedGuid; }))
				{
					OutGuid = ParsedGuid;
					return true;
				}
				OutFailReason = FString::Printf(TEXT("'%s' does not match any node in this graph"), *NodeRef);
				return false;
			}
			int32 MatchCount = 0;
			for (const FMcpNodeSnapshot& Snapshot : GraphNodes)
			{
				if (Snapshot.Name.Equals(NodeRef, ESearchCase::IgnoreCase) ||
					Snapshot.ClassName.Equals(NodeRef, ESearchCase::IgnoreCase))
				{
					OutGuid = Snapshot.Id;
					++MatchCount;
				}
			}
			if (MatchCount == 1)
			{
				return true;
			}
			OutFailReason = MatchCount > 1
				? FString::Printf(TEXT("'%s' matches %d nodes - use the node GUID"), *NodeRef, MatchCount)
				: FString::Printf(TEXT("'%s' is not a GUID and does not match any node name"), *NodeRef);
			return false;
		};

		FString SourceFailReason;
		FString TargetFailReason;
		const bool bSourceResolved = ResolveNodeReference(SourceNodeId, SourceGuid, SourceFailReason);
		const bool bTargetResolved = ResolveNodeReference(TargetNodeId, TargetGuid, TargetFailReason);
		if (!bSourceResolved || !bTargetResolved)
		{
			TArray<FString> Reasons;
			if (!SourceFailReason.IsEmpty()) { Reasons.Add(FString::Printf(TEXT("sourceNodeId: %s"), *SourceFailReason)); }
			if (!TargetFailReason.IsEmpty()) { Reasons.Add(FString::Printf(TEXT("targetNodeId: %s"), *TargetFailReason)); }
			TSharedPtr<FJsonObject> ErrorResponse = McpHandlerUtils::BuildErrorResponse(
				TEXT("INVALID_GUID"),
				FString::Printf(TEXT("Invalid node reference - %s"), *FString::Join(Reasons, TEXT("; "))));
			TArray<TSharedPtr<FJsonValue>> NodeArray = BuildAvailableNodesArray();
			if (NodeArray.Num() > 0)
			{
				ErrorResponse->SetArrayField(TEXT("availableNodes"), NodeArray);
			}
			return ErrorResponse;
		}
#else
		if (!FGuid::Parse(SourceNodeId, SourceGuid) || !FGuid::Parse(TargetNodeId, TargetGuid))
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("INVALID_GUID"), TEXT("Invalid node ID format - must be valid GUID"));
		}
#endif

		Metasound::Frontend::FNamedEdge NamedEdge{SourceGuid, FName(*SourceOutputName), TargetGuid, FName(*TargetInputName)};
		TSet<Metasound::Frontend::FNamedEdge> Edges;
		Edges.Add(NamedEdge);
		TArray<const FMetasoundFrontendEdge*> CreatedEdges;
		bool bSuccess = Builder.AddNamedEdges(Edges, &CreatedEdges, true);

		if (!bSuccess && (SourceOutputName.IsEmpty() || SourceOutputName.Equals(TEXT("Value"))))
		{
			const FMcpNodeSnapshot* SrcSnapshot = GraphNodes.FindByPredicate(
				[&SourceGuid](const FMcpNodeSnapshot& Snap) { return Snap.Id == SourceGuid; });
			if (SrcSnapshot && SrcSnapshot->Outputs.Num() == 1)
			{
				FString ResolvedOutput = SrcSnapshot->Outputs[0];
				int32 SpaceIdx = ResolvedOutput.Find(TEXT(" ("));
				if (SpaceIdx != INDEX_NONE) { ResolvedOutput = ResolvedOutput.Left(SpaceIdx); }
				TSet<Metasound::Frontend::FNamedEdge> AliasEdges;
				AliasEdges.Add(Metasound::Frontend::FNamedEdge{SourceGuid, FName(*ResolvedOutput), TargetGuid, FName(*TargetInputName)});
				CreatedEdges.Reset();
				bSuccess = Builder.AddNamedEdges(AliasEdges, &CreatedEdges, true);
			}
		}

		bool bConnected = CreatedEdges.Num() > 0;
#if MCP_HAS_METASOUND_FRONTEND_V2
		// Replacing an input's existing connection swaps the last edge into the removed
		// slot, so AddNamedEdges lists no created edge although it made one: this used to
		// answer EDGE_FAILED, skip the save and leave the rewire in memory. Read it back.
		if (bSuccess && !bConnected)
		{
			if (const FMetasoundFrontendVertex* Input = Builder.FindNodeInput(TargetGuid, FName(*TargetInputName)))
			{
				const FMetasoundFrontendNode* ConnectedNode = nullptr;
				bConnected = Builder.FindNodeOutputConnectedToNodeInput(TargetGuid, Input->VertexID, &ConnectedNode) &&
					ConnectedNode && ConnectedNode->GetID() == SourceGuid;
				Response->SetBoolField(TEXT("replacedConnection"), bConnected);
			}
		}
#endif
		if (bSuccess && bConnected)
		{
			McpSafeAssetSave(MetaSound);
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("message"), TEXT("MetaSound nodes connected"));
			Response->SetNumberField(TEXT("edgesCreated"), FMath::Max(1, CreatedEdges.Num()));
			// Echo the resolved GUIDs so name-based callers learn the canonical IDs.
			Response->SetStringField(TEXT("sourceNodeId"), SourceGuid.ToString());
			Response->SetStringField(TEXT("targetNodeId"), TargetGuid.ToString());
			McpHandlerUtils::AddVerification(Response, MetaSound);
		}
		else
		{
			Response->SetBoolField(TEXT("success"), false);
			Response->SetStringField(TEXT("error"), TEXT("Failed to create edge connection - check pin names against availableNodes inputs/outputs"));
			Response->SetStringField(TEXT("errorCode"), TEXT("EDGE_FAILED"));
			Response->SetStringField(TEXT("code"), TEXT("EDGE_FAILED"));
#if MCP_HAS_METASOUND_FRONTEND_V2
			TArray<TSharedPtr<FJsonValue>> NodeIdArray = BuildAvailableNodesArray();
			if (NodeIdArray.Num() > 0)
			{
				Response->SetArrayField(TEXT("availableNodes"), NodeIdArray);
			}
#endif
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
}
#endif
