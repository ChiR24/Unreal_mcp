// get_metasound_graph: the MetaSound graph as data. Changing one note used to mean reading
// RootMetaSoundDocument through inspect.get_property (15-40 KB of UE export text per asset,
// with edges given as bare vertex GUIDs) just to find a node id for set_default.
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AudioAuthoring/McpAutomationBridge_AudioAuthoringHandlersPrivate.h"

namespace McpAudioAuthoring
{
#if MCP_HAS_METASOUND && MCP_HAS_METASOUND_FRONTEND_V2
namespace
{
FString MetaSoundClassTypeName(EMetasoundFrontendClassType Type)
{
	switch (Type)
	{
	case EMetasoundFrontendClassType::External: return TEXT("Node");
	case EMetasoundFrontendClassType::Graph: return TEXT("Graph");
	case EMetasoundFrontendClassType::Input: return TEXT("GraphInput");
	case EMetasoundFrontendClassType::Output: return TEXT("GraphOutput");
	case EMetasoundFrontendClassType::Literal: return TEXT("Literal");
	case EMetasoundFrontendClassType::Variable:
	case EMetasoundFrontendClassType::VariableDeferredAccessor:
	case EMetasoundFrontendClassType::VariableAccessor:
	case EMetasoundFrontendClassType::VariableMutator: return TEXT("Variable");
	case EMetasoundFrontendClassType::Template: return TEXT("Template");
	default: return TEXT("Invalid");
	}
}

TArray<TSharedPtr<FJsonValue>> MetaSoundClassVertices(const TArray<FMetasoundFrontendClassInput>& Inputs)
{
	TArray<TSharedPtr<FJsonValue>> Out;
	for (const FMetasoundFrontendClassInput& Vertex : Inputs)
	{
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("name"), Vertex.Name.ToString());
		Entry->SetStringField(TEXT("type"), Vertex.TypeName.ToString());
		Entry->SetStringField(TEXT("nodeId"), Vertex.NodeID.ToString());
		Out.Add(MakeShared<FJsonValueObject>(Entry));
	}
	return Out;
}

TArray<TSharedPtr<FJsonValue>> MetaSoundClassVertices(const TArray<FMetasoundFrontendClassOutput>& Outputs)
{
	TArray<TSharedPtr<FJsonValue>> Out;
	for (const FMetasoundFrontendClassOutput& Vertex : Outputs)
	{
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("name"), Vertex.Name.ToString());
		Entry->SetStringField(TEXT("type"), Vertex.TypeName.ToString());
		Entry->SetStringField(TEXT("nodeId"), Vertex.NodeID.ToString());
		Out.Add(MakeShared<FJsonValueObject>(Entry));
	}
	return Out;
}
}
#endif

TSharedPtr<FJsonObject> HandleMetaSoundGraphReadAction(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
	if (SubAction != TEXT("get_metasound_graph"))
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
	TScriptInterface<IMetaSoundDocumentInterface> ScriptInterface(MetaSound);
	const FMetasoundFrontendDocument& Doc = ScriptInterface->GetConstDocument();

	// Vertex ids are shared by every node of a class, so pins are keyed by (node, vertex).
	TMap<FGuid, const FMetasoundFrontendNode*> NodesById;
	TMap<TPair<FGuid, FGuid>, FString> PinNames;
	TArray<TSharedPtr<FJsonValue>> Nodes;
	TArray<TSharedPtr<FJsonValue>> Edges;
	Doc.RootGraph.IterateGraphPages([&](const FMetasoundFrontendGraph& Page)
	{
		for (const FMetasoundFrontendNode& Node : Page.Nodes)
		{
			NodesById.Add(Node.GetID(), &Node);
			const FMetasoundFrontendClass* Class = Doc.Dependencies.FindByPredicate(
				[&Node](const FMetasoundFrontendClass& Dependency) { return Dependency.ID == Node.ClassID; });
			TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("nodeId"), Node.GetID().ToString());
			Entry->SetStringField(TEXT("name"), Node.Name.ToString());
			Entry->SetStringField(TEXT("className"), Class ? Class->Metadata.GetClassName().ToString() : FString());
			Entry->SetStringField(TEXT("kind"), MetaSoundClassTypeName(Class ? Class->Metadata.GetType() : EMetasoundFrontendClassType::Invalid));
			TArray<TSharedPtr<FJsonValue>> Inputs;
			for (const FMetasoundFrontendVertex& Input : Node.Interface.Inputs)
			{
				PinNames.Add(TPair<FGuid, FGuid>(Node.GetID(), Input.VertexID), Input.Name.ToString());
				TSharedPtr<FJsonObject> Pin = MakeShared<FJsonObject>();
				Pin->SetStringField(TEXT("name"), Input.Name.ToString());
				Pin->SetStringField(TEXT("type"), Input.TypeName.ToString());
				if (const FMetasoundFrontendVertexLiteral* Literal = Node.InputLiterals.FindByPredicate(
					[&Input](const FMetasoundFrontendVertexLiteral& Candidate) { return Candidate.VertexID == Input.VertexID; }))
				{
					Pin->SetStringField(TEXT("literal"), Literal->Value.ToString());
				}
				Inputs.Add(MakeShared<FJsonValueObject>(Pin));
			}
			Entry->SetArrayField(TEXT("inputs"), Inputs);
			TArray<TSharedPtr<FJsonValue>> Outputs;
			for (const FMetasoundFrontendVertex& Output : Node.Interface.Outputs)
			{
				PinNames.Add(TPair<FGuid, FGuid>(Node.GetID(), Output.VertexID), Output.Name.ToString());
				TSharedPtr<FJsonObject> Pin = MakeShared<FJsonObject>();
				Pin->SetStringField(TEXT("name"), Output.Name.ToString());
				Pin->SetStringField(TEXT("type"), Output.TypeName.ToString());
				Outputs.Add(MakeShared<FJsonValueObject>(Pin));
			}
			Entry->SetArrayField(TEXT("outputs"), Outputs);
			Nodes.Add(MakeShared<FJsonValueObject>(Entry));
		}
		for (const FMetasoundFrontendEdge& Edge : Page.Edges)
		{
			TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
			const FMetasoundFrontendNode* const* From = NodesById.Find(Edge.FromNodeID);
			const FMetasoundFrontendNode* const* To = NodesById.Find(Edge.ToNodeID);
			const FString* FromPin = PinNames.Find(TPair<FGuid, FGuid>(Edge.FromNodeID, Edge.FromVertexID));
			const FString* ToPin = PinNames.Find(TPair<FGuid, FGuid>(Edge.ToNodeID, Edge.ToVertexID));
			Entry->SetStringField(TEXT("fromNodeId"), Edge.FromNodeID.ToString());
			Entry->SetStringField(TEXT("fromNode"), From ? (*From)->Name.ToString() : FString());
			Entry->SetStringField(TEXT("fromPin"), FromPin ? *FromPin : Edge.FromVertexID.ToString());
			Entry->SetStringField(TEXT("toNodeId"), Edge.ToNodeID.ToString());
			Entry->SetStringField(TEXT("toNode"), To ? (*To)->Name.ToString() : FString());
			Entry->SetStringField(TEXT("toPin"), ToPin ? *ToPin : Edge.ToVertexID.ToString());
			Edges.Add(MakeShared<FJsonValueObject>(Entry));
		}
	});

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
	const FMetasoundFrontendClassInterface& GraphInterface = Doc.RootGraph.GetDefaultInterface();
#else
	const FMetasoundFrontendClassInterface& GraphInterface = Doc.RootGraph.Interface;
#endif
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("message"), FString::Printf(TEXT("%d nodes, %d links"), Nodes.Num(), Edges.Num()));
	Response->SetStringField(TEXT("assetPath"), MetaSound->GetPathName());
	Response->SetNumberField(TEXT("nodeCount"), Nodes.Num());
	Response->SetNumberField(TEXT("edgeCount"), Edges.Num());
	Response->SetArrayField(TEXT("nodes"), Nodes);
	Response->SetArrayField(TEXT("edges"), Edges);
	Response->SetArrayField(TEXT("graphInputs"), MetaSoundClassVertices(GraphInterface.Inputs));
	Response->SetArrayField(TEXT("graphOutputs"), MetaSoundClassVertices(GraphInterface.Outputs));
	return Response;
#else
	return McpHandlerUtils::BuildErrorResponse(TEXT("METASOUND_FRONTEND_NOT_SUPPORTED"),
		TEXT("Reading a MetaSound graph needs the MetaSound document API of UE 5.5 or later."));
#endif
}
}
