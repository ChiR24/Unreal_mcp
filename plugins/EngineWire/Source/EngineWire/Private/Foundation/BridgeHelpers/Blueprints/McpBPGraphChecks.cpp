#include "Foundation/BridgeHelpers/Blueprints/McpBPGraphChecks.h"

#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet/KismetSystemLibrary.h"

void McpAddCheckWarning(TArray<TSharedPtr<FJsonValue>>& Diagnostics, const FString& Message)
{
  TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
  Entry->SetStringField(TEXT("severity"), TEXT("warning"));
  Entry->SetStringField(TEXT("message"), Message);
  Diagnostics.Add(MakeShared<FJsonValueObject>(Entry));
}

FString McpNodeLabel(const UEdGraphNode* Node)
{
  return FString::Printf(TEXT("'%s' (%s)"), *Node->GetNodeTitle(ENodeTitleType::ListView).ToString(), *Node->GetName());
}

// A Set of HomeX wired to a spawned copy, beside a Get of this one's HomeX, read as this object's HomeX set twice.
bool McpVariableOnSelf(const UEdGraphNode* Node)
{
  const UK2Node_Variable* Variable = Cast<UK2Node_Variable>(Node);
  const UEdGraphPin* Target = Variable ? Variable->FindPin(UEdGraphSchema_K2::PN_Self) : nullptr;
  return Variable && Variable->VariableReference.IsSelfContext() && (!Target || Target->LinkedTo.Num() == 0);
}

namespace
{
bool McpIsPureNode(const UEdGraphNode* Node)
{
  const UK2Node* K2 = Cast<UK2Node>(Node);
  return K2 && K2->IsNodePure();
}

// The nodes that do something with Node's value: its impure readers, directly or through pure nodes.
void McpCollectImpureReaders(const UEdGraphNode* Node, TSet<const UEdGraphNode*>& Readers, int32 Depth)
{
  if (Depth > 16) return;
  for (const UEdGraphPin* Pin : Node->Pins)
  {
    if (!Pin || Pin->Direction != EGPD_Output || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) continue;
    for (const UEdGraphPin* Link : Pin->LinkedTo)
    {
      const UEdGraphNode* Reader = Link ? Link->GetOwningNode() : nullptr;
      if (Reader && McpIsPureNode(Reader))
      {
        McpCollectImpureReaders(Reader, Readers, Depth + 1);
      }
      else if (Reader)
      {
        Readers.Add(Reader);
      }
    }
  }
}

// Whether a pure node takes the Blueprint's own Variable through its inputs, directly or through other pure nodes.
bool McpPureReadsVariable(const UEdGraphNode* Node, FName Variable, int32 Depth)
{
  if (const UK2Node_VariableGet* Get = Cast<UK2Node_VariableGet>(Node))
  {
    return Get->GetVarName() == Variable && McpVariableOnSelf(Get);
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

// The Blueprint's own variable a node changes: its Set node's, or the one a call takes by reference and changes in
// place (Map Add, Array Add, Remove Index). OutWritePin is that reference pin; NAME_None when the node writes none.
FName McpWrittenVariable(const UEdGraphNode* Node, const UEdGraphPin*& OutWritePin)
{
  OutWritePin = nullptr;
  if (const UK2Node_VariableSet* Set = Cast<UK2Node_VariableSet>(Node))
  {
    return McpVariableOnSelf(Set) ? Set->GetVarName() : NAME_None;
  }
  if (McpIsPureNode(Node))
  {
    return NAME_None;
  }
  for (const UEdGraphPin* Pin : Node->Pins)
  {
    const UK2Node_VariableGet* Get = Pin && Pin->Direction == EGPD_Input && Pin->PinType.bIsReference && Pin->LinkedTo.Num() == 1
                                         ? Cast<UK2Node_VariableGet>(Pin->LinkedTo[0]->GetOwningNode()) : nullptr;
    if (Get && McpVariableOnSelf(Get))
    {
      OutWritePin = Pin;
      return Get->GetVarName();
    }
  }
  return NAME_None;
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
    for (const UEdGraphNode* Writer : Graph->Nodes)
    {
      const UEdGraphPin* WritePin = nullptr;
      const FName Variable = Writer ? McpWrittenVariable(Writer, WritePin) : NAME_None;
      if (Variable.IsNone()) continue;
      TSet<const UEdGraphNode*> Reach, Reported;
      for (const UEdGraphPin* Input : Writer->Pins)
      {
        if (!Input || Input->Direction != EGPD_Input || Input == WritePin) continue;
        for (const UEdGraphPin* Link : Input->LinkedTo)
        {
          const UEdGraphNode* Source = Link ? Link->GetOwningNode() : nullptr;
          if (!McpIsPureNode(Source) || Reported.Contains(Source) || !McpPureReadsVariable(Source, Variable, 0)) continue;
          if (Reach.Num() == 0) McpCollectExecReach(Writer, Reach);
          const UEdGraphNode* Late = McpFindLateReader(Source, Writer, Reach);
          if (!Late || Diagnostics.Num() >= MaxDiagnostics) continue;
          Reported.Add(Source);
          const FString Name = Variable.ToString();
          const FString Into = WritePin ? FString::Printf(TEXT("%s, which changes %s in place,"), *McpNodeLabel(Writer), *Name)
                                        : FString::Printf(TEXT("Set %s"), *Name);
          McpAddCheckWarning(Diagnostics, FString::Printf(
              TEXT("%s in %s reads %s and is wired into %s and into %s, which runs after it: a pure node runs again for each "
                   "node that reads it, so '%s' gets the new %s worked on once more. %s"),
              *McpNodeLabel(Source), *Graph->GetName(), *Name, *Into, *McpNodeLabel(Late),
              *Late->GetNodeTitle(ENodeTitleType::ListView).ToString(), *Name,
              WritePin ? TEXT("To use the value written, store it with a Set node first and read that variable.")
                       : TEXT("To use the value just set, wire it from the Set node's output pin, Output_Get.")));
        }
      }
    }
  }
}

void McpAddRandomReadWarnings(const UBlueprint* Blueprint, TArray<TSharedPtr<FJsonValue>>& Diagnostics, int32 MaxDiagnostics)
{
  if (!Blueprint) return;
  TArray<UEdGraph*> Graphs;
  Blueprint->GetAllGraphs(Graphs);
  for (const UEdGraph* Graph : Graphs)
  {
    if (!Graph) continue;
    for (const UEdGraphNode* Node : Graph->Nodes)
    {
      const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
      const UFunction* Function = Call && Call->IsNodePure() ? Call->GetTargetFunction() : nullptr;
      if (!Function || !Function->GetName().Contains(TEXT("Random"))) continue;
      TSet<const UEdGraphNode*> Readers;
      McpCollectImpureReaders(Call, Readers, 0);
      if (Readers.Num() < 2 || Diagnostics.Num() >= MaxDiagnostics) continue;
      TArray<FString> Labels;
      for (const UEdGraphNode* Reader : Readers)
      {
        Labels.Add(McpNodeLabel(Reader));
      }
      Labels.Sort();
      McpAddCheckWarning(Diagnostics, FString::Printf(
          TEXT("%s in %s is read by %d nodes that run separately (%s): a pure node runs again for each node that reads it, "
               "so each gets its own random value. To give them one value, store it with a Set node first and read that "
               "variable."),
          *McpNodeLabel(Call), *Graph->GetName(), Readers.Num(), *FString::Join(Labels, TEXT(", "))));
    }
  }
}

void McpAddTimerNameWarnings(const UBlueprint* Blueprint, TArray<TSharedPtr<FJsonValue>>& Diagnostics, int32 MaxDiagnostics)
{
  const UClass* Class = Blueprint ? Blueprint->GeneratedClass.Get() : nullptr;
  if (!Class) return;
  TArray<UEdGraph*> Graphs;
  Blueprint->GetAllGraphs(Graphs);
  for (const UEdGraph* Graph : Graphs)
  {
    if (!Graph) continue;
    for (const UEdGraphNode* Node : Graph->Nodes)
    {
      const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
      const UFunction* Function = Call ? Call->GetTargetFunction() : nullptr;
      const UEdGraphPin* NamePin = Function && Function->GetOwnerClass() == UKismetSystemLibrary::StaticClass()
                                       ? Call->FindPin(TEXT("FunctionName")) : nullptr;
      const UEdGraphPin* ObjectPin = NamePin ? Call->FindPin(TEXT("Object")) : nullptr;
      // An unwired Object is the Blueprint itself; another object's functions are not this check's to judge.
      if (!ObjectPin || NamePin->LinkedTo.Num() > 0 || ObjectPin->LinkedTo.Num() > 0 || ObjectPin->DefaultObject) continue;
      const FString Name = NamePin->DefaultValue.TrimStartAndEnd();
      if (Name.IsEmpty() || Class->FindFunctionByName(FName(*Name)) || Diagnostics.Num() >= MaxDiagnostics) continue;
      TArray<FString> Own;
      for (TFieldIterator<UFunction> It(Class, EFieldIteratorFlags::ExcludeSuper); It && Own.Num() < 8; ++It)
      {
        if (!It->HasAnyFunctionFlags(FUNC_Delegate) && !It->GetName().StartsWith(TEXT("ExecuteUbergraph")))
        {
          Own.Add(It->GetName());
        }
      }
      McpAddCheckWarning(Diagnostics, FString::Printf(
          TEXT("%s in %s names '%s', and this Blueprint has no function or event by that name, so the timer never runs. "
               "Name one it has%s."),
          *McpNodeLabel(Call), *Graph->GetName(), *Name,
          Own.Num() > 0 ? *FString::Printf(TEXT(" (%s)"), *FString::Join(Own, TEXT(", "))) : TEXT("")));
    }
  }
}
