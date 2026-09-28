// remove_metasound_node and disconnect_metasound_nodes: the two edits a MetaSound
// graph was missing. Without them, re-voicing a synth left the old oscillators in the
// graph as dead nodes (MS_UIClick carried 8) and a wrong link could only be replaced.
// Both are all or nothing: every node, pin and link is checked before the graph changes.
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AudioAuthoring/McpAutomationBridge_AudioAuthoringHandlersPrivate.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"

namespace McpAudioAuthoring
{
#if MCP_HAS_METASOUND && MCP_HAS_METASOUND_FRONTEND_V2
namespace
{
// Graph input and output nodes (On Play, Out Mono, graph inputs) belong to the MetaSound's
// interface: removing one breaks the source, so they are refused.
bool IsMetaSoundInterfaceNode(const FMetasoundFrontendDocument& Doc, const FMetasoundFrontendNode& Node)
{
	const FMetasoundFrontendClass* Class = Doc.Dependencies.FindByPredicate(
		[&Node](const FMetasoundFrontendClass& Dependency) { return Dependency.ID == Node.ClassID; });
	const EMetasoundFrontendClassType Type = Class ? Class->Metadata.GetType() : EMetasoundFrontendClassType::Invalid;
	return Type == EMetasoundFrontendClassType::Input || Type == EMetasoundFrontendClassType::Output;
}

TSharedPtr<FJsonObject> MetaSoundGraphRefuse(const TCHAR* Code, const FString& Message)
{
	return McpHandlerUtils::BuildErrorResponse(Code, Message);
}

TSharedPtr<FJsonObject> RemoveMetaSoundNodes(UMetaSoundSource* MetaSound, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
	TArray<FString> Refs;
	const FString Single = GetJsonStringField(Params, TEXT("nodeId"), TEXT(""));
	if (!Single.IsEmpty()) { Refs.Add(Single); }
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (Params->TryGetArrayField(TEXT("nodeIds"), Values) && Values)
	{
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			FString Ref;
			if (Value.IsValid() && Value->TryGetString(Ref) && !Ref.IsEmpty()) { Refs.AddUnique(Ref); }
		}
	}
	if (Refs.Num() == 0)
	{
		return MetaSoundGraphRefuse(TEXT("INVALID_ARGUMENT"), TEXT("Give nodeId for one node or nodeIds for several (the ids add_node returned, or from get_metasound_graph)."));
	}

	TScriptInterface<IMetaSoundDocumentInterface> ScriptInterface(MetaSound);
	MCP_METASOUND_BUILDER(Builder, ScriptInterface);
	const FMetasoundFrontendDocument& Doc = Builder.GetConstDocumentChecked();
	TArray<FGuid> Ids;
	TArray<FString> Problems;
	for (const FString& Ref : Refs)
	{
		FGuid Id;
		const FMetasoundFrontendNode* Node = FGuid::Parse(Ref, Id) ? Builder.FindNode(Id) : nullptr;
		if (!Node) { Problems.Add(FString::Printf(TEXT("'%s' matches no node"), *Ref)); }
		else if (IsMetaSoundInterfaceNode(Doc, *Node)) { Problems.Add(FString::Printf(TEXT("'%s' (%s) is a graph input or output"), *Ref, *Node->Name.ToString())); }
		else { Ids.Add(Id); }
	}
	if (Problems.Num() > 0)
	{
		MCP_METASOUND_FINISH(Builder);
		return MetaSoundGraphRefuse(TEXT("NODE_NOT_REMOVABLE"), FString::Printf(TEXT("Nothing was removed: %s. Read the ids with get_metasound_graph."),
			*FString::Join(Problems, TEXT("; "))));
	}

	TArray<TSharedPtr<FJsonValue>> Removed;
	for (const FGuid& Id : Ids)
	{
		if (Builder.RemoveNode(Id)) { Removed.Add(MakeShared<FJsonValueString>(Id.ToString())); }
	}
	const bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
	const bool bSaved = Removed.Num() > 0 && bSave && McpSafeAssetSave(MetaSound);
	MCP_METASOUND_FINISH(Builder);
	if (Removed.Num() != Ids.Num())
	{
		return MetaSoundGraphRefuse(TEXT("REMOVE_FAILED"), FString::Printf(TEXT("Removed %d of %d nodes; the builder refused the rest."), Removed.Num(), Ids.Num()));
	}
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Removed %d MetaSound node(s) and their links"), Removed.Num()));
	Response->SetArrayField(TEXT("removed"), Removed);
	Response->SetNumberField(TEXT("removedCount"), Removed.Num());
	Response->SetBoolField(TEXT("saved"), bSaved);
	McpHandlerUtils::AddVerification(Response, MetaSound);
	return Response;
}

TSharedPtr<FJsonObject> DisconnectMetaSoundPins(UMetaSoundSource* MetaSound, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
	const FString TargetRef = GetJsonStringField(Params, TEXT("targetNodeId"), TEXT(""));
	const FString TargetPin = GetJsonStringField(Params, TEXT("targetInputName"), TEXT(""));
	const FString SourceRef = GetJsonStringField(Params, TEXT("sourceNodeId"), TEXT(""));
	const FString SourcePin = GetJsonStringField(Params, TEXT("sourceOutputName"), TEXT(""));
	const bool bByTarget = !TargetRef.IsEmpty() && !TargetPin.IsEmpty();
	const bool bBySource = !SourceRef.IsEmpty() && !SourcePin.IsEmpty();
	if (!bByTarget && !bBySource)
	{
		return MetaSoundGraphRefuse(TEXT("INVALID_ARGUMENT"), TEXT("Give targetNodeId + targetInputName (the link into that input) or sourceNodeId + sourceOutputName (every link out of that output)."));
	}

	TScriptInterface<IMetaSoundDocumentInterface> ScriptInterface(MetaSound);
	MCP_METASOUND_BUILDER(Builder, ScriptInterface);
	FGuid TargetId, SourceId;
	const FMetasoundFrontendVertex* Input = bByTarget && FGuid::Parse(TargetRef, TargetId) ? Builder.FindNodeInput(TargetId, FName(*TargetPin)) : nullptr;
	const FMetasoundFrontendVertex* Output = bBySource && FGuid::Parse(SourceRef, SourceId) ? Builder.FindNodeOutput(SourceId, FName(*SourcePin)) : nullptr;
	FString Problem;
	const TCHAR* ProblemCode = TEXT("PIN_NOT_FOUND");
	if (bByTarget && !Input) { Problem = FString::Printf(TEXT("node '%s' has no input '%s'"), *TargetRef, *TargetPin); }
	else if (bBySource && !Output) { Problem = FString::Printf(TEXT("node '%s' has no output '%s'"), *SourceRef, *SourcePin); }
	int32 Removed = 0;
	if (Problem.IsEmpty() && bByTarget)
	{
		const FMetasoundFrontendNode* Connected = nullptr;
		const FMetasoundFrontendVertex* From = Builder.FindNodeOutputConnectedToNodeInput(TargetId, Input->VertexID, &Connected);
		// With both ends given, the one link into the input must come from that output.
		if (!From || !Connected)
		{
			ProblemCode = TEXT("NOT_CONNECTED");
			Problem = FString::Printf(TEXT("input '%s' has no link"), *TargetPin);
		}
		else if (bBySource && (Connected->GetID() != SourceId || From->VertexID != Output->VertexID))
		{
			ProblemCode = TEXT("LINK_MISMATCH");
			Problem = FString::Printf(TEXT("input '%s' is linked from %s.%s, not from the given source"), *TargetPin, *Connected->Name.ToString(), *From->Name.ToString());
		}
		else if (Builder.RemoveEdgeToNodeInput(TargetId, Input->VertexID)) { Removed = 1; }
		else { ProblemCode = TEXT("DISCONNECT_FAILED"); Problem = TEXT("the builder refused the removal"); }
	}
	else if (Problem.IsEmpty())
	{
		Removed = Builder.FindNodeInputsConnectedToNodeOutput(SourceId, Output->VertexID).Num();
		if (Removed == 0) { ProblemCode = TEXT("NOT_CONNECTED"); Problem = FString::Printf(TEXT("output '%s' has no link"), *SourcePin); }
		else if (!Builder.RemoveEdgesFromNodeOutput(SourceId, Output->VertexID))
		{
			Removed = 0;
			ProblemCode = TEXT("DISCONNECT_FAILED");
			Problem = TEXT("the builder refused the removal");
		}
	}
	if (!Problem.IsEmpty())
	{
		MCP_METASOUND_FINISH(Builder);
		return MetaSoundGraphRefuse(ProblemCode, FString::Printf(TEXT("Nothing was disconnected: %s."), *Problem));
	}
	const bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
	const bool bSaved = bSave && McpSafeAssetSave(MetaSound);
	MCP_METASOUND_FINISH(Builder);
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Removed %d MetaSound link(s)"), Removed));
	Response->SetNumberField(TEXT("edgesRemoved"), Removed);
	Response->SetBoolField(TEXT("saved"), bSaved);
	McpHandlerUtils::AddVerification(Response, MetaSound);
	return Response;
}
}
#endif

TSharedPtr<FJsonObject> HandleMetaSoundGraphEditActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
	const bool bRemove = SubAction == TEXT("remove_metasound_node");
	if (!bRemove && SubAction != TEXT("disconnect_metasound_nodes"))
	{
		return nullptr;
	}
#if MCP_HAS_METASOUND && MCP_HAS_METASOUND_FRONTEND_V2
	const FString AssetPath = NormalizeAudioPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
	if (AssetPath.IsEmpty())
	{
		return McpHandlerUtils::BuildErrorResponse(TEXT("MISSING_PATH"), TEXT("Asset path is required"));
	}
	UMetaSoundSource* MetaSound = Cast<UMetaSoundSource>(StaticLoadObject(UMetaSoundSource::StaticClass(), nullptr, *AssetPath));
	if (!MetaSound)
	{
		return McpHandlerUtils::BuildErrorResponse(TEXT("ASSET_NOT_FOUND"), FString::Printf(TEXT("Could not load MetaSound: %s"), *AssetPath));
	}
	return bRemove ? RemoveMetaSoundNodes(MetaSound, Params, Response) : DisconnectMetaSoundPins(MetaSound, Params, Response);
#else
	return McpHandlerUtils::BuildErrorResponse(TEXT("METASOUND_FRONTEND_NOT_SUPPORTED"),
		TEXT("Removing MetaSound nodes and links needs the MetaSound document builder of UE 5.5 or later."));
#endif
}
}
