#include "Foundation/BridgeHelpers/Blueprints/McpBPStaleReadCheck.h"

#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"

namespace
{
bool McpIsPureNode(const UEdGraphNode* Node)
{
  const UK2Node* K2 = Cast<UK2Node>(Node);
  return K2 && K2->IsNodePure();
}

// Whether a pure node takes the Blueprint's own Variable through its inputs, directly or through other pure nodes.
bool McpPureReadsVariable(const UEdGraphNode* Node, FName Variable, int32 Depth)
{
  if (const UK2Node_VariableGet* Get = Cast<UK2Node_VariableGet>(Node))
  {
    return Get->GetVarName() == Variable && Get->VariableReference.IsSelfContext();
  }
  if (!McpIsPureNode(Node) || Depth > 16) return false;
  for (const UEdGraphPin* Pin : Node->Pins)
  {
    if (!Pin || Pin->Direction != EGPD_Input) continue;
    for (const UEdGraphPin* Link : Pin->LinkedTo)
    {
      if (Link && McpPureReadsVariable(Link->GetOwningNode(), Variable, Depth + 1)) return true;
    }
  }
  return false;
}

// The nodes the exec wires reach from Start's exec outputs: what runs after it.
void McpCollectExecReach(const UEdGraphNode* Start, TSet<const UEdGraphNode*>& Reach)
{
  TArray<const UEdGraphNode*> Queue{Start};
  while (Queue.Num() > 0 && Reach.Num() < 512)
  {
    const UEdGraphNode* Node = Queue.Pop();
    for (const UEdGraphPin* Pin : Node->Pins)
    {
      if (!Pin || Pin->Direction != EGPD_Output || Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec) continue;
      for (const UEdGraphPin* Link : Pin->LinkedTo)
      {
        const UEdGraphNode* Next = Link ? Link->GetOwningNode() : nullptr;
        if (Next && !Reach.Contains(Next))
        {
          Reach.Add(Next);
          Queue.Add(Next);
        }
      }
    }
  }
}

// Whether Node runs after the Set, or is a pure node that one of those reads.
bool McpFeedsReach(const UEdGraphNode* Node, const TSet<const UEdGraphNode*>& Reach, int32 Depth)
{
  if (!Node || Reach.Contains(Node)) return Node != nullptr;
  if (!McpIsPureNode(Node) || Depth > 16) return false;
  for (const UEdGraphPin* Pin : Node->Pins)
  {
    if (!Pin || Pin->Direction != EGPD_Output) continue;
    for (const UEdGraphPin* Link : Pin->LinkedTo)
    {
      if (Link && McpFeedsReach(Link->GetOwningNode(), Reach, Depth + 1)) return true;
    }
  }
  return false;
}

// A node other than the Set that reads Source and runs after the Set, or null.
const UEdGraphNode* McpFindLateReader(const UEdGraphNode* Source, const UEdGraphNode* Set,
                                      const TSet<const UEdGraphNode*>& Reach)
{
  for (const UEdGraphPin* Pin : Source->Pins)
  {
    if (!Pin || Pin->Direction != EGPD_Output) continue;
    for (const UEdGraphPin* Link : Pin->LinkedTo)
    {
      const UEdGraphNode* Reader = Link ? Link->GetOwningNode() : nullptr;
      if (Reader && Reader != Set && McpFeedsReach(Reader, Reach, 0)) return Reader;
    }
  }
  return nullptr;
}
}

void McpAddStaleReadWarnings(const UBlueprint* Blueprint, TArray<TSharedPtr<FJsonValue>>& Diagnostics, int32 MaxDiagnostics)
{
  if (!Blueprint) return;
  TArray<UEdGraph*> Graphs;
  Blueprint->GetAllGraphs(Graphs);
  for (const UEdGraph* Graph : Graphs)
  {
    if (!Graph) continue;
    for (const UEdGraphNode* Node : Graph->Nodes)
    {
      const UK2Node_VariableSet* Set = Cast<UK2Node_VariableSet>(Node);
      const FName Variable = Set ? Set->GetVarName() : NAME_None;
      const UEdGraphPin* Value = Set && Set->VariableReference.IsSelfContext() ? Set->FindPin(Variable, EGPD_Input) : nullptr;
      if (!Value) continue;
      TSet<const UEdGraphNode*> Reach;
      for (const UEdGraphPin* Link : Value->LinkedTo)
      {
        const UEdGraphNode* Source = Link ? Link->GetOwningNode() : nullptr;
        if (!McpIsPureNode(Source) || !McpPureReadsVariable(Source, Variable, 0)) continue;
        if (Reach.Num() == 0) McpCollectExecReach(Set, Reach);
        const UEdGraphNode* Late = McpFindLateReader(Source, Set, Reach);
        if (!Late || Diagnostics.Num() >= MaxDiagnostics) continue;
        const FString Name = Variable.ToString();
        const FString SourceTitle = Source->GetNodeTitle(ENodeTitleType::ListView).ToString();
        const FString LateTitle = Late->GetNodeTitle(ENodeTitleType::ListView).ToString();
        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("severity"), TEXT("warning"));
        Entry->SetStringField(TEXT("message"), FString::Printf(
            TEXT("'%s' (%s in %s) reads %s and is wired into Set %s and into '%s' (%s), which runs after that Set: a pure "
                 "node runs again for each node that reads it, so '%s' gets the new %s worked on once more. To use the "
                 "value just set, wire it from the Set node's output pin, Output_Get."),
            *SourceTitle, *Source->GetName(), *Graph->GetName(), *Name, *Name, *LateTitle, *Late->GetName(), *LateTitle, *Name));
        Diagnostics.Add(MakeShared<FJsonValueObject>(Entry));
      }
    }
  }
}
