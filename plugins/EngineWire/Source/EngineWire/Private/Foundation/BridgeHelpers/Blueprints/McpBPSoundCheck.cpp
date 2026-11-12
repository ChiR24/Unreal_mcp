#include "Foundation/BridgeHelpers/Blueprints/McpBPSoundCheck.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPGraphChecks.h"

#include "Core/Compatibility/McpMetaSoundCompat.h"

#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

// Why a sound never stops by itself, or empty when it does: a looping wave or cue and a MetaSound without the
// one-shot interface loop, and a one-shot MetaSound whose On Finished output nothing fires plays on just the same
// (its graph is read on 5.5+).
static FString DescribeEndlessSound(const USoundBase* Sound)
{
  if (!Sound)
  {
    return FString();
  }
  if (Sound->IsLooping())
  {
    return TEXT("loops");
  }
#if MCP_HAS_METASOUND && MCP_HAS_METASOUND_FRONTEND_V2
  // A one-shot MetaSound stops when its On Finished output fires; a hiss or hum built without wiring it never does.
  // Read only; TScriptInterface takes a mutable object.
  UMetaSoundSource* MetaSound = const_cast<UMetaSoundSource*>(Cast<UMetaSoundSource>(Sound));
  if (!MetaSound)
  {
    return FString();
  }
  TScriptInterface<IMetaSoundDocumentInterface> ScriptInterface(MetaSound);
  const FMetasoundFrontendDocument& Doc = ScriptInterface->GetConstDocument();
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
  const FMetasoundFrontendClassInterface& GraphInterface = Doc.RootGraph.GetDefaultInterface();
#else
  const FMetasoundFrontendClassInterface& GraphInterface = Doc.RootGraph.Interface;
#endif
  const FName OnFinished(TEXT("UE.Source.OneShot.OnFinished"));
  const FMetasoundFrontendClassOutput* Finished = GraphInterface.Outputs.FindByPredicate(
    [&OnFinished](const FMetasoundFrontendClassOutput& Output) { return Output.Name == OnFinished; });
  if (!Finished)
  {
    return FString();
  }
  bool bFired = false;
  Doc.RootGraph.IterateGraphPages([&bFired, Finished](const FMetasoundFrontendGraph& Page)
  {
    bFired = bFired || Page.Edges.ContainsByPredicate(
      [Finished](const FMetasoundFrontendEdge& Edge) { return Edge.ToNodeID == Finished->NodeID; });
  });
  if (!bFired)
  {
    return TEXT("is a one-shot MetaSound that never fires On Finished");
  }
#endif
  return FString();
}

void McpAddSoundNodeWarnings(const UBlueprint* Blueprint, TArray<TSharedPtr<FJsonValue>>& Diagnostics, int32 MaxDiagnostics)
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
      const UFunction* Function = Call ? Call->GetTargetFunction() : nullptr;
      if (!Function || Function->GetOwnerClass() != UGameplayStatics::StaticClass()) continue;
      const FName Name = Function->GetFName();
      const bool bAtLocation = Name == TEXT("PlaySoundAtLocation") || Name == TEXT("SpawnSoundAtLocation") || Name == TEXT("SpawnSoundAttached");
      const bool bForgotten = Name == TEXT("PlaySoundAtLocation") || Name == TEXT("PlaySound2D");
      const UEdGraphPin* SoundPin = Call->FindPin(TEXT("Sound"));
      const USoundBase* Sound = SoundPin && SoundPin->LinkedTo.Num() == 0 ? Cast<USoundBase>(SoundPin->DefaultObject) : nullptr;
      if (!Sound) continue;
      const FString Where = FString::Printf(TEXT("%s (%s in %s) plays %s"), *Name.ToString(), *Node->GetName(), *Graph->GetName(), *Sound->GetName());
      const UEdGraphPin* AttenuationPin = Call->FindPin(TEXT("AttenuationSettings"));
      if (bAtLocation && AttenuationPin && AttenuationPin->LinkedTo.Num() == 0 && !AttenuationPin->DefaultObject
          && !Sound->GetAttenuationSettingsToApply() && Diagnostics.Num() < MaxDiagnostics)
      {
        McpAddCheckWarning(Diagnostics, Where + TEXT(" with no attenuation, so it is heard at full volume at any distance: set "
            "the node's Attenuation Settings, or give the sound attenuation."));
      }
      const FString Endless = bForgotten ? DescribeEndlessSound(Sound) : FString();
      if (!Endless.IsEmpty() && Diagnostics.Num() < MaxDiagnostics)
      {
        McpAddCheckWarning(Diagnostics, FString::Printf(TEXT("%s, which %s: nothing can stop it, so every call adds a copy that "
            "plays until the level unloads. Use SpawnSound2D or SpawnSoundAtLocation and keep the AudioComponent it returns, "
            "or play the sound from an AudioComponent on the actor."), *Where, *Endless));
      }
    }
  }
}
