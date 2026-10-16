#include "Foundation/BridgeHelpers/Blueprints/McpBPGraphChecks.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_BaseAsyncTask.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_Timeline.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"

namespace
{
bool McpIsExecPin(const UEdGraphPin* Pin) { return Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec; }

bool McpHasExecPin(const UEdGraphNode* Node)
{
  for (const UEdGraphPin* Pin : Node->Pins)
  {
    if (McpIsExecPin(Pin)) return true;
  }
  return false;
}

// Leaving Node through Out runs later: every output of a latent call or a timeline, an async task's besides then.
bool McpWaitsThrough(const UEdGraphNode* Node, const UEdGraphPin* Out)
{
  const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
  if ((Call && Call->IsLatentFunction()) || Node->IsA<UK2Node_Timeline>()) return true;
  return Node->IsA<UK2Node_BaseAsyncTask>() && Out->PinName != UEdGraphSchema_K2::PN_Then;
}

// What every run of the event executes before anything can stop it: the event, then its one-way chain up to and
// including the first node that branches (a Cast too) or waits. A macro may hold the run back, so it ends the chain.
TSet<const UEdGraphNode*> McpEveryRunPrefix(const UEdGraphNode* Event)
{
  TSet<const UEdGraphNode*> Prefix{Event};
  const UEdGraphNode* Node = Event;
  for (int32 Step = 0; Step < 64; ++Step)
  {
    const UEdGraphPin* Next = nullptr;
    int32 Outs = 0;
    for (const UEdGraphPin* Pin : Node->Pins)
    {
      if (McpIsExecPin(Pin) && Pin->Direction == EGPD_Output)
      {
        ++Outs;
        Next = Pin;
      }
    }
    if (Outs != 1 || Next->LinkedTo.Num() != 1 || McpWaitsThrough(Node, Next)) break;
    Node = Next->LinkedTo[0] ? Next->LinkedTo[0]->GetOwningNode() : nullptr;
    if (!Node || Node->IsA<UK2Node_MacroInstance>()) break;
    Prefix.Add(Node);
  }
  return Prefix;
}

// The prefix node that writes Input's value on every run, through pure nodes: one whose own output it is, or the Set of
// a variable it reads (a variable set before the wait is written again by the next run too). What names the value.
const UEdGraphNode* McpEveryRunWriter(const UEdGraphPin* Input, const TSet<const UEdGraphNode*>& Prefix, FString& What)
{
  TArray<const UEdGraphPin*> Queue{Input};
  TSet<const UEdGraphNode*> Seen;
  while (Queue.Num() > 0 && Seen.Num() < 64)
  {
    for (const UEdGraphPin* Link : Queue.Pop()->LinkedTo)
    {
      const UEdGraphNode* Node = Link ? Link->GetOwningNode() : nullptr;
      if (!Node || Seen.Contains(Node)) continue;
      Seen.Add(Node);
      What = Link->PinName.ToString();
      if (Prefix.Contains(Node)) return Node;
      const UK2Node_VariableGet* Get = Cast<UK2Node_VariableGet>(Node);
      if (Get && McpVariableOnSelf(Get))
      {
        for (const UEdGraphNode* Writer : Prefix)
        {
          const UK2Node_VariableSet* Set = Cast<UK2Node_VariableSet>(Writer);
          if (Set && McpVariableOnSelf(Set) && Set->GetVarName() == Get->GetVarName()) return Set;
        }
      }
      // An impure node holds what it last computed, and this chain ran it after the prefix: not a prefix value.
      if (McpHasExecPin(Node)) continue;
      for (const UEdGraphPin* In : Node->Pins)
      {
        if (In && In->Direction == EGPD_Input && !McpIsExecPin(In) && In->LinkedTo.Num() > 0) Queue.Add(In);
      }
    }
  }
  return nullptr;
}

// A node on an exec path, with the wait it is behind (null before any).
struct FMcpExecStep
{
  const UEdGraphNode* Node;
  const UEdGraphNode* Waited;
};
}

void McpAddWaitRereadWarnings(const UBlueprint* Blueprint, TArray<TSharedPtr<FJsonValue>>& Diagnostics, int32 MaxDiagnostics)
{
  if (!Blueprint) return;
  TArray<UEdGraph*> Graphs;
  Blueprint->GetAllGraphs(Graphs);
  for (const UEdGraph* Graph : Graphs)
  {
    if (!Graph) continue;
    for (const UEdGraphNode* Entry : Graph->Nodes)
    {
      // Any event can run again while its chain waits, but the ones a Blueprint gets once.
      const UK2Node_Event* Event = Cast<UK2Node_Event>(Entry);
      const FName Name = Event ? Event->EventReference.GetMemberName() : NAME_None;
      if (!Event || Name == FName(TEXT("ReceiveBeginPlay")) || Name == FName(TEXT("ReceiveEndPlay")) ||
          Name == FName(TEXT("ReceiveDestroyed")))
      {
        continue;
      }
      const TSet<const UEdGraphNode*> Prefix = McpEveryRunPrefix(Event);
      TArray<FMcpExecStep> Queue;
      Queue.Add({Event, nullptr});
      TSet<const UEdGraphNode*> Seen{Event};
      while (Queue.Num() > 0 && Seen.Num() < 512 && Diagnostics.Num() < MaxDiagnostics)
      {
        const FMcpExecStep Item = Queue.Pop();
        for (const UEdGraphPin* Out : Item.Node->Pins)
        {
          if (!McpIsExecPin(Out) || Out->Direction != EGPD_Output) continue;
          const UEdGraphNode* Waited = Item.Waited ? Item.Waited : (McpWaitsThrough(Item.Node, Out) ? Item.Node : nullptr);
          for (const UEdGraphPin* Link : Out->LinkedTo)
          {
            const UEdGraphNode* Node = Link ? Link->GetOwningNode() : nullptr;
            if (!Node || Seen.Contains(Node)) continue;
            Seen.Add(Node);
            Queue.Add({Node, Waited});
            if (!Waited) continue;
            for (const UEdGraphPin* In : Node->Pins)
            {
              if (!In || In->Direction != EGPD_Input || McpIsExecPin(In)) continue;
              FString What;
              const UEdGraphNode* Writer = McpEveryRunWriter(In, Prefix, What);
              if (!Writer || Diagnostics.Num() >= MaxDiagnostics) continue;
              McpAddCheckWarning(Diagnostics, FString::Printf(
                  TEXT("%s in %s reads %s after the wait at %s, and %s writes it on every run of %s. A run that comes "
                       "while the chain waits writes it again (a failed cast leaves None), so the read gets that run's "
                       "value. Write what the wait needs only in the run that starts it, behind a Branch on a flag or a "
                       "Do Once that keeps later runs out, and read that after the wait."),
                  *McpNodeLabel(Node), *Graph->GetName(), *What, *McpNodeLabel(Waited), *McpNodeLabel(Writer),
                  *McpNodeLabel(Event)));
              break;
            }
          }
        }
      }
    }
  }
}
