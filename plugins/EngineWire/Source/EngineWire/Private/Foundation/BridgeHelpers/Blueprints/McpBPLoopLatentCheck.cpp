#include "Foundation/BridgeHelpers/Blueprints/McpBPGraphChecks.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"

namespace
{
// The first latent call the exec wires reach from Pin: what a loop body runs, up to its first wait.
const UK2Node_CallFunction* McpFirstLatentReached(const UEdGraphPin* Pin)
{
  TArray<const UEdGraphPin*> Queue{Pin};
  TSet<const UEdGraphNode*> Seen;
  while (Queue.Num() > 0 && Seen.Num() < 512)
  {
    for (const UEdGraphPin* Link : Queue.Pop()->LinkedTo)
    {
      const UEdGraphNode* Node = Link ? Link->GetOwningNode() : nullptr;
      if (!Node || Seen.Contains(Node)) continue;
      Seen.Add(Node);
      const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
      if (Call && Call->IsLatentFunction()) return Call;
      for (const UEdGraphPin* Out : Node->Pins)
      {
        if (Out && Out->Direction == EGPD_Output && Out->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) Queue.Add(Out);
      }
    }
  }
  return nullptr;
}
}

void McpAddLoopLatentWarnings(const UBlueprint* Blueprint, TArray<TSharedPtr<FJsonValue>>& Diagnostics, int32 MaxDiagnostics)
{
  if (!Blueprint) return;
  TArray<UEdGraph*> Graphs;
  Blueprint->GetAllGraphs(Graphs);
  TSet<const UK2Node_CallFunction*> Reported;
  for (const UEdGraph* Graph : Graphs)
  {
    if (!Graph) continue;
    for (const UEdGraphNode* Loop : Graph->Nodes)
    {
      // ForLoop, ForEachLoop, WhileLoop and For Each <Enum> all name their body pin so.
      const UEdGraphPin* Body = Loop ? Loop->FindPin(TEXT("LoopBody"), EGPD_Output) : nullptr;
      if (!Body && Loop) Body = Loop->FindPin(TEXT("Loop Body"), EGPD_Output);
      const UK2Node_CallFunction* Latent = Body ? McpFirstLatentReached(Body) : nullptr;
      if (!Latent || Reported.Contains(Latent) || Diagnostics.Num() >= MaxDiagnostics) continue;
      Reported.Add(Latent);
      McpAddCheckWarning(Diagnostics, FString::Printf(
          TEXT("%s in %s waits inside the loop body of %s: the loop runs every pass at once and the node holds one wait "
               "per Blueprint, so later passes start no wait of their own and what follows the node runs once, after the "
               "loop ends. To space the passes out, drive them from a looping timer, or from an event that counts up and "
               "calls itself after the wait, in place of the loop."),
          *McpNodeLabel(Latent), *Graph->GetName(), *McpNodeLabel(Loop)));
    }
  }
}
