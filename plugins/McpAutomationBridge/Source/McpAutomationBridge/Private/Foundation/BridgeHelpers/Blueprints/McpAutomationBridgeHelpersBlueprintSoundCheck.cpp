#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintSoundCheck.h"

#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

void McpAddUnattenuatedSoundWarnings(const UBlueprint* Blueprint, TArray<TSharedPtr<FJsonValue>>& Diagnostics, int32 MaxDiagnostics)
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
      if (Name != TEXT("PlaySoundAtLocation") && Name != TEXT("SpawnSoundAtLocation") && Name != TEXT("SpawnSoundAttached")) continue;
      const UEdGraphPin* AttenuationPin = Call->FindPin(TEXT("AttenuationSettings"));
      const UEdGraphPin* SoundPin = Call->FindPin(TEXT("Sound"));
      if (!AttenuationPin || !SoundPin || AttenuationPin->LinkedTo.Num() > 0 || AttenuationPin->DefaultObject) continue;
      const USoundBase* Sound = SoundPin->LinkedTo.Num() == 0 ? Cast<USoundBase>(SoundPin->DefaultObject) : nullptr;
      if (!Sound || Sound->GetAttenuationSettingsToApply() || Diagnostics.Num() >= MaxDiagnostics) continue;
      TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
      Entry->SetStringField(TEXT("severity"), TEXT("warning"));
      Entry->SetStringField(TEXT("message"), FString::Printf(TEXT("%s (%s in %s) plays %s with no attenuation, so it is heard "
          "at full volume at any distance: set the node's Attenuation Settings, or give the sound attenuation."),
          *Name.ToString(), *Node->GetName(), *Graph->GetName(), *Sound->GetName()));
      Diagnostics.Add(MakeShared<FJsonValueObject>(Entry));
    }
  }
}
