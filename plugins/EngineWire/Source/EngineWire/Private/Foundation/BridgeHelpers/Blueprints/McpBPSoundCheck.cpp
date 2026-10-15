#include "Foundation/BridgeHelpers/Blueprints/McpBPSoundCheck.h"

#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Domains/AudioAuthoring/MetaSound/McpAudioSoundEnds.h"

static void McpAddSoundWarning(TArray<TSharedPtr<FJsonValue>>& Diagnostics, const FString& Message)
{
  TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
  Entry->SetStringField(TEXT("severity"), TEXT("warning"));
  Entry->SetStringField(TEXT("message"), Message);
  Diagnostics.Add(MakeShared<FJsonValueObject>(Entry));
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
        McpAddSoundWarning(Diagnostics, Where + TEXT(" with no attenuation, so it is heard at full volume at any distance: set "
            "the node's Attenuation Settings, or give the sound attenuation."));
      }
      const FString Endless = bForgotten ? McpAudioAuthoring::DescribeEndlessSound(Sound) : FString();
      if (!Endless.IsEmpty() && Diagnostics.Num() < MaxDiagnostics)
      {
        McpAddSoundWarning(Diagnostics, FString::Printf(TEXT("%s, which %s: nothing can stop it, so every call adds a copy that "
            "plays until the level unloads. Use SpawnSound2D or SpawnSoundAtLocation and keep the AudioComponent it returns, "
            "or play the sound from an AudioComponent on the actor."), *Where, *Endless));
      }
    }
  }
}
