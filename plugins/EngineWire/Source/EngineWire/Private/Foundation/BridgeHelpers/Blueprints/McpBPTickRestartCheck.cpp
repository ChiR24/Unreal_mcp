#include "Foundation/BridgeHelpers/Blueprints/McpBPGraphChecks.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_MultiGate.h"
#include "K2Node_Switch.h"
#include "Kismet/KismetSystemLibrary.h"

namespace
{
// A call that starts its wait over each time it runs: a timer set again, or a Retriggerable Delay.
bool McpRestartsWait(const UK2Node_CallFunction* Call)
{
  const UFunction* Function = Call ? Call->GetTargetFunction() : nullptr;
  if (!Function || Function->GetOwnerClass() != UKismetSystemLibrary::StaticClass()) return false;
  const FName Name = Function->GetFName();
  return Name == GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, K2_SetTimer) ||
         Name == GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, K2_SetTimerDelegate) ||
         Name == GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, RetriggerableDelay);
}

// A node that may let only some frames through: a branch, a switch, a gate or Do Once macro, or a wait.
bool McpMayHoldBack(const UEdGraphNode* Node)
{
  const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
  return Node->IsA<UK2Node_IfThenElse>() || Node->IsA<UK2Node_Switch>() || Node->IsA<UK2Node_MacroInstance>() ||
         Node->IsA<UK2Node_MultiGate>() || (Call && Call->IsLatentFunction());
}

// The first call reached from Tick through nodes that pass every frame on, that starts its wait over.
const UK2Node_CallFunction* McpFindEveryFrameRestart(const UEdGraphNode* Tick)
{
  TArray<const UEdGraphNode*> Queue{Tick};
  TSet<const UEdGraphNode*> Seen{Tick};
  while (Queue.Num() > 0 && Seen.Num() < 512)
  {
    for (const UEdGraphPin* Pin : Queue.Pop()->Pins)
    {
      if (!Pin || Pin->Direction != EGPD_Output || Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec) continue;
      for (const UEdGraphPin* Link : Pin->LinkedTo)
      {
        const UEdGraphNode* Node = Link ? Link->GetOwningNode() : nullptr;
        if (!Node || Seen.Contains(Node)) continue;
        Seen.Add(Node);
        const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
        if (McpRestartsWait(Call)) return Call;
        if (!McpMayHoldBack(Node)) Queue.Add(Node);
      }
    }
  }
  return nullptr;
}
}

void McpAddTickRestartWarnings(const UBlueprint* Blueprint, TArray<TSharedPtr<FJsonValue>>& Diagnostics, int32 MaxDiagnostics)
{
  if (!Blueprint) return;
  TArray<UEdGraph*> Graphs;
  Blueprint->GetAllGraphs(Graphs);
  for (const UEdGraph* Graph : Graphs)
  {
    if (!Graph) continue;
    for (const UEdGraphNode* Node : Graph->Nodes)
    {
      // An actor's or component's Event Tick, or a widget's; a custom event is no override.
      const UK2Node_Event* Tick = Cast<UK2Node_Event>(Node);
      const FName Event = Tick && Tick->bOverrideFunction ? Tick->EventReference.GetMemberName() : NAME_None;
      if (Event != FName(TEXT("ReceiveTick")) && Event != FName(TEXT("Tick"))) continue;
      const UK2Node_CallFunction* Restart = McpFindEveryFrameRestart(Tick);
      if (!Restart || Diagnostics.Num() >= MaxDiagnostics) continue;
      McpAddCheckWarning(Diagnostics, FString::Printf(
          TEXT("%s in %s runs on every frame from %s, and each call starts its wait over, so a wait longer than a frame "
               "never ends: the timer never fires, or nothing after the delay runs. Start it once, from BeginPlay or "
               "behind a Do Once or a Branch on a flag."),
          *McpNodeLabel(Restart), *Graph->GetName(), *McpNodeLabel(Tick)));
    }
  }
}
