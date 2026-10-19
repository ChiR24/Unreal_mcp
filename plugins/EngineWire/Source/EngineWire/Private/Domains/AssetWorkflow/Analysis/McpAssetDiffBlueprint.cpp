// diff_asset's Blueprint and DataTable parts: class defaults, components (the construction script's nodes, by
// variable name) and graphs (by their path below the Blueprint, through the engine's graph diff); DataTable rows by
// row name. The other side's values read through FMcpAssetDiff::Normalize.

#include "Domains/AssetWorkflow/Analysis/McpAssetDiff.h"

#include "DiffResults.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GraphDiffControl.h"

namespace
{
TSharedPtr<FJsonObject> AssetDiffRow(const TCHAR* Key, const FString& Name, const TCHAR* Change)
{
	TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
	Row->SetStringField(Key, Name);
	Row->SetStringField(TEXT("change"), Change);
	return Row;
}

TSharedPtr<FJsonObject> AssetDiffRow(const TCHAR* Key, const FString& Name, const TCHAR* Change, const FString& Now, const FString& Saved)
{
	TSharedPtr<FJsonObject> Row = AssetDiffRow(Key, Name, Change);
	if (!Now.IsEmpty()) Row->SetStringField(TEXT("now"), Now);
	if (!Saved.IsEmpty()) Row->SetStringField(TEXT("saved"), Saved);
	return Row;
}

// A variable added, removed or retyped stays on the Blueprint's own list until it compiles, so the class defaults
// alone missed it and an uncompiled edit read as identical. Defaults are left to the class defaults compare.
void AssetDiffVariables(FMcpAssetDiff& Diff, const UBlueprint* Live, const UBlueprint* Other)
{
	auto Type = [](const FBPVariableDescription& Var) { return UEdGraphSchema_K2::TypeToText(Var.VarType).ToString(); };
	auto Find = [](const UBlueprint* Blueprint, FName Name)
	{
		return Blueprint->NewVariables.FindByPredicate([Name](const FBPVariableDescription& Each) { return Each.VarName == Name; });
	};
	for (const FBPVariableDescription& Var : Live->NewVariables)
	{
		const FBPVariableDescription* Saved = Find(Other, Var.VarName);
		if (!Saved) Diff.Add(Diff.Properties, AssetDiffRow(TEXT("variable"), Var.VarName.ToString(), TEXT("added"), Type(Var), FString()));
		else if (Type(*Saved) != Type(Var)) Diff.Add(Diff.Properties, AssetDiffRow(TEXT("variable"), Var.VarName.ToString(), TEXT("type changed"), Type(Var), Type(*Saved)));
	}
	for (const FBPVariableDescription& Var : Other->NewVariables)
	{
		if (!Find(Live, Var.VarName)) Diff.Add(Diff.Properties, AssetDiffRow(TEXT("variable"), Var.VarName.ToString(), TEXT("removed")));
	}
}

TMap<FName, USCS_Node*> AssetDiffNodes(const UBlueprint* Blueprint)
{
	TMap<FName, USCS_Node*> Nodes;
	if (Blueprint->SimpleConstructionScript)
	{
		for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
		{
			if (Node) Nodes.Add(Node->GetVariableName(), Node);
		}
	}
	return Nodes;
}

void AssetDiffComponents(FMcpAssetDiff& Diff, const UBlueprint* Live, const UBlueprint* Other)
{
	TMap<FName, USCS_Node*> Saved = AssetDiffNodes(Other);
	for (const TPair<FName, USCS_Node*>& Entry : AssetDiffNodes(Live))
	{
		const FString Name = Entry.Key.ToString();
		const USCS_Node* Node = Entry.Value;
		USCS_Node* Match = nullptr;
		if (!Saved.RemoveAndCopyValue(Entry.Key, Match))
		{
			Diff.Add(Diff.Components, AssetDiffRow(TEXT("component"), Name, TEXT("added"), GetNameSafe(Node->ComponentClass), FString()));
			continue;
		}
		const FString NowClass = Diff.Normalize(GetNameSafe(Node->ComponentClass));
		const FString SavedClass = Diff.Normalize(GetNameSafe(Match->ComponentClass));
		if (!NowClass.Equals(SavedClass, ESearchCase::CaseSensitive))
		{
			Diff.Add(Diff.Components, AssetDiffRow(TEXT("component"), Name, TEXT("class"), NowClass, SavedClass));
		}
		if (Node->ParentComponentOrVariableName != Match->ParentComponentOrVariableName)
		{
			Diff.Add(Diff.Components, AssetDiffRow(TEXT("component"), Name, TEXT("parent"),
				Node->ParentComponentOrVariableName.ToString(), Match->ParentComponentOrVariableName.ToString()));
		}
		if (Node->ComponentTemplate && Match->ComponentTemplate)
		{
			Diff.DiffContainers(TEXT("component ") + Name, Node->ComponentTemplate->GetClass(), Node->ComponentTemplate,
				Match->ComponentTemplate->GetClass(), Match->ComponentTemplate);
		}
	}
	for (const TPair<FName, USCS_Node*>& Entry : Saved)
	{
		Diff.Add(Diff.Components, AssetDiffRow(TEXT("component"), Entry.Key.ToString(), TEXT("removed"),
			FString(), GetNameSafe(Entry.Value->ComponentClass)));
	}
}

TMap<FString, UEdGraph*> AssetDiffGraphMap(UBlueprint* Blueprint)
{
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	TMap<FString, UEdGraph*> Out;
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph) Out.Add(Graph->GetPathName(Blueprint), Graph);
	}
	return Out;
}

void AssetDiffGraphs(FMcpAssetDiff& Diff, UBlueprint* Live, UBlueprint* Other)
{
	TMap<FString, UEdGraph*> Saved = AssetDiffGraphMap(Other);
	for (const TPair<FString, UEdGraph*>& Entry : AssetDiffGraphMap(Live))
	{
		UEdGraph* Graph = Entry.Value;
		UEdGraph* Match = nullptr;
		if (!Saved.RemoveAndCopyValue(Entry.Key, Match))
		{
			Diff.Add(Diff.Graphs, AssetDiffRow(TEXT("graph"), Graph->GetName(), TEXT("graph added")));
			continue;
		}
		TArray<FDiffSingleResult> Results;
		FGraphDiffControl::DiffGraphs(Match, Graph, Results);
		for (const FDiffSingleResult& Result : Results)
		{
			if (Result.Diff == EDiffType::NODE_MOVED) continue;
			const TCHAR* Change = Result.Diff == EDiffType::NODE_ADDED ? TEXT("node added")
				: Result.Diff == EDiffType::NODE_REMOVED ? TEXT("node removed") : TEXT("changed");
			TSharedPtr<FJsonObject> Row = AssetDiffRow(TEXT("graph"), Graph->GetName(), Change);
			// The live graph's node where there is one, so its id works with the graph tools.
			const UEdGraphNode* LiveNode = Result.Node2 && Result.Node2->GetGraph() == Graph ? Result.Node2
				: Result.Node1 && Result.Node1->GetGraph() == Graph ? Result.Node1 : nullptr;
			const UEdGraphNode* Named = LiveNode ? LiveNode : Result.Node1;
			if (Named) Row->SetStringField(TEXT("node"), Named->GetNodeTitle(ENodeTitleType::ListView).ToString());
			if (LiveNode) Row->SetStringField(TEXT("nodeId"), LiveNode->NodeGuid.ToString());
			Row->SetStringField(TEXT("detail"), Result.DisplayString.ToString());
			Diff.Add(Diff.Graphs, Row);
		}
	}
	for (const TPair<FString, UEdGraph*>& Entry : Saved)
	{
		Diff.Add(Diff.Graphs, AssetDiffRow(TEXT("graph"), Entry.Value->GetName(), TEXT("graph removed")));
	}
}
} // namespace

void McpAssetDiff::DiffBlueprint(FMcpAssetDiff& Diff, UBlueprint* Live, UBlueprint* Other)
{
	if (!Other) return;
	// The class defaults: each variable's default and every inherited setting the Blueprint overrides. A component's
	// own variable is always None there; componentChanges reports it.
	UClass* LiveClass = Live->GeneratedClass;
	UClass* OtherClass = Other->GeneratedClass;
	if (LiveClass && OtherClass)
	{
		for (const TPair<FName, USCS_Node*>& Entry : AssetDiffNodes(Live)) Diff.Skip.Add(Entry.Key);
		for (const TPair<FName, USCS_Node*>& Entry : AssetDiffNodes(Other)) Diff.Skip.Add(Entry.Key);
		Diff.DiffContainers(TEXT("defaults"), LiveClass, LiveClass->GetDefaultObject(), OtherClass, OtherClass->GetDefaultObject());
		Diff.Skip.Reset();
	}
	AssetDiffVariables(Diff, Live, Other);
	AssetDiffComponents(Diff, Live, Other);
	AssetDiffGraphs(Diff, Live, Other);
}

void McpAssetDiff::DiffDataTable(FMcpAssetDiff& Diff, UDataTable* Live, UDataTable* Other)
{
	const UDataTable* LiveTable = Live;
	const UDataTable* OtherTable = Other;
	const UScriptStruct* LiveStruct = LiveTable->GetRowStruct();
	const UScriptStruct* OtherStruct = OtherTable ? OtherTable->GetRowStruct() : nullptr;
	if (!LiveStruct || !OtherStruct) return;
	const TMap<FName, uint8*>& LiveRows = LiveTable->GetRowMap();
	const TMap<FName, uint8*>& SavedRows = OtherTable->GetRowMap();
	for (const TPair<FName, uint8*>& Row : LiveRows)
	{
		uint8* const* Match = SavedRows.Find(Row.Key);
		if (Match)
		{
			Diff.DiffContainers(TEXT("row ") + Row.Key.ToString(), LiveStruct, Row.Value, OtherStruct, *Match);
		}
		else
		{
			Diff.Add(Diff.Rows, AssetDiffRow(TEXT("row"), Row.Key.ToString(), TEXT("added")));
		}
	}
	for (const TPair<FName, uint8*>& Row : SavedRows)
	{
		if (!LiveRows.Contains(Row.Key)) Diff.Add(Diff.Rows, AssetDiffRow(TEXT("row"), Row.Key.ToString(), TEXT("removed")));
	}
}
