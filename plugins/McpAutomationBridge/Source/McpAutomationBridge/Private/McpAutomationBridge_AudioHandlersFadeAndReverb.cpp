#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AudioHandlersPrivate.h"

namespace McpAudioHandlers
{
#if WITH_EDITOR
bool HandleFadeAndReverbActions(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const FString& Lower,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
   if (Lower == TEXT("fade_sound") || Lower == TEXT("audio_fade_sound")) {
     FString ActorName;
     Payload->TryGetStringField(TEXT("soundName"), ActorName);
     if (ActorName.IsEmpty()) {
       Payload->TryGetStringField(TEXT("actorName"), ActorName);
     }
     double FadeTime = 1.0;
     Payload->TryGetNumberField(TEXT("fadeTime"), FadeTime);
      double TargetVolume = 0.0;
      Payload->TryGetNumberField(TEXT("targetVolume"), TargetVolume);
      FString FadeType = TEXT("FadeTo");
      Payload->TryGetStringField(TEXT("fadeType"), FadeType);

     if (!GEditor) {
       Self->SendAutomationError(RequestingSocket, RequestId,
                           TEXT("Editor not available"), TEXT("NO_EDITOR"));
       return true;
     }
     UWorld *World = GEditor->GetEditorWorldContext().World();
     if (!World) {
       Self->SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                           TEXT("NO_WORLD"));
       return true;
     }

	AActor *TargetActor = FindAudioActorByName(ActorName, World);
	if (!TargetActor) {
		Self->SendAutomationError(RequestingSocket, RequestId,
			TEXT("Actor not found"), TEXT("ACTOR_NOT_FOUND"));
		return true;
	}

	FString ComponentName;
	Payload->TryGetStringField(TEXT("componentName"), ComponentName);
	UAudioComponent *AudioComp = nullptr;

	if (!ComponentName.IsEmpty())
	{
		TArray<UAudioComponent*> Components;
		TargetActor->GetComponents<UAudioComponent>(Components);
		for (UAudioComponent* Comp : Components)
		{
			if (Comp && (Comp->GetName() == ComponentName || Comp->GetFName().ToString() == ComponentName))
			{
				AudioComp = Comp;
				break;
			}
		}
	}

	if (!AudioComp)
	{
		AudioComp = TargetActor->FindComponentByClass<UAudioComponent>();
	}

	if (!AudioComp)
	{
		TArray<UActorComponent*> AllComps;
		TargetActor->GetComponents(AllComps);
		for (UActorComponent* Comp : AllComps)
		{
			if (Comp && Comp->IsA<UAudioComponent>())
			{
				AudioComp = Cast<UAudioComponent>(Comp);
				break;
			}
		}
	}

	if (!AudioComp)
	{
		bool bFound = false;
		ForEachObjectOfClass(UAudioComponent::StaticClass(), [&](UObject* Obj)
		{
			if (bFound) return;
			UAudioComponent* Comp = Cast<UAudioComponent>(Obj);
			if (Comp && Comp->GetOwner() == TargetActor)
			{
				AudioComp = Comp;
				bFound = true;
			}
		}, true, RF_ClassDefaultObject);
	}

	if (!AudioComp) {
		Self->SendAutomationError(RequestingSocket, RequestId,
			TEXT("Audio component not found on actor"),
			TEXT("COMPONENT_NOT_FOUND"));
		return true;
	}

	// Execute fade based on type
     if (FadeType.Equals(TEXT("FadeIn"), ESearchCase::IgnoreCase)) {
       AudioComp->FadeIn((float)FadeTime, (float)TargetVolume);
     } if (FadeType.Equals(TEXT("FadeOut"), ESearchCase::IgnoreCase)) {
       AudioComp->FadeOut((float)FadeTime, (float)TargetVolume);
     } else {
       // FadeTo: Adjust volume over time
       AudioComp->FadeIn((float)FadeTime, (float)TargetVolume);
     }

     TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
     Resp->SetBoolField(TEXT("success"), true);
     Resp->SetStringField(TEXT("actorName"), ActorName);
	Resp->SetStringField(TEXT("action"), Lower);
     McpHandlerUtils::AddVerification(Resp, TargetActor);
     Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                            TEXT("Sound fading"), Resp);
     return true;
   }

   // -------------------------------------------------------------------------
   // create_reverb_zone
   // -------------------------------------------------------------------------
   // Creates an AAudioVolume actor with reverb settings.
   //
   // Payload:  { "name": string (required), "location"?: [x,y,z],
   //             "size"?: [x,y,z], "reverbEffect"?: string (asset path),
   //             "volume"?: number, "fadeTime"?: number }
   // Response: { "success": bool, "actorName": string, "location": {x,y,z} }
   // -------------------------------------------------------------------------
   if (Lower == TEXT("create_reverb_zone") || Lower == TEXT("audio_create_reverb_zone")) {
     FString ZoneName;
     if (!Payload->TryGetStringField(TEXT("name"), ZoneName) || ZoneName.IsEmpty()) {
       Self->SendAutomationError(RequestingSocket, RequestId,
                           TEXT("name required"), TEXT("INVALID_ARGUMENT"));
       return true;
     }

     FVector Location = FVector::ZeroVector;
     const TArray<TSharedPtr<FJsonValue>> *LocArr;
     if (Payload->TryGetArrayField(TEXT("location"), LocArr) && LocArr && LocArr->Num() >= 3) {
       Location = FVector((*LocArr)[0]->AsNumber(), (*LocArr)[1]->AsNumber(),
                          (*LocArr)[2]->AsNumber());
     }

     FVector Size = FVector(500.0f, 500.0f, 500.0f);
     const TArray<TSharedPtr<FJsonValue>> *SizeArr;
     if (Payload->TryGetArrayField(TEXT("size"), SizeArr) && SizeArr && SizeArr->Num() >= 3) {
       Size = FVector((*SizeArr)[0]->AsNumber(), (*SizeArr)[1]->AsNumber(),
                      (*SizeArr)[2]->AsNumber());
     }

     FString ReverbEffectPath;
     Payload->TryGetStringField(TEXT("reverbEffect"), ReverbEffectPath);
     double Volume = 1.0;
     Payload->TryGetNumberField(TEXT("volume"), Volume);
     double FadeTime = 2.0;
     Payload->TryGetNumberField(TEXT("fadeTime"), FadeTime);

     if (!GEditor) {
       Self->SendAutomationError(RequestingSocket, RequestId,
                           TEXT("Editor not available"), TEXT("NO_EDITOR"));
       return true;
     }
     UWorld *World = GEditor->GetEditorWorldContext().World();
      if (!World) {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                            TEXT("NO_WORLD"));
        return true;
      }

      // Check for existing actor with same name (name collision detection)
      AActor* ExistingActor = FindAudioActorByName(ZoneName, World);
      if (ExistingActor) {
        Self->SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Actor '%s' already exists in level"), *ZoneName),
                            TEXT("DUPLICATE_NAME"));
        return true;
      }

      // Spawn AudioVolume actor
      FActorSpawnParameters SpawnParams;
      SpawnParams.Name = FName(*ZoneName);
      AAudioVolume *AudioVolume = World->SpawnActor<AAudioVolume>(Location, FRotator::ZeroRotator, SpawnParams);
     if (!AudioVolume) {
       Self->SendAutomationError(RequestingSocket, RequestId,
                           TEXT("Failed to spawn AudioVolume"), TEXT("SPAWN_FAILED"));
       return true;
     }

     // Set actor label
     AudioVolume->SetActorLabel(ZoneName);

      // Configure brush bounds
      if (UBrushComponent *BrushComp = AudioVolume->GetBrushComponent()) {
        // Set volume bounds via brush
        BrushComp->SetRelativeLocation(FVector::ZeroVector);
      }

      // Create reverb settings and apply via public API
      FReverbSettings ReverbSettings;
      ReverbSettings.bApplyReverb = true;

      // Load and apply reverb effect if provided
      if (!ReverbEffectPath.IsEmpty()) {
        UReverbEffect *ReverbEffect = LoadObject<UReverbEffect>(nullptr, *ReverbEffectPath);
        if (ReverbEffect) {
          ReverbSettings.ReverbEffect = ReverbEffect;
        }
      }

      // Set volume settings
      ReverbSettings.Volume = (float)Volume;
      ReverbSettings.FadeTime = (float)FadeTime;

      // Apply settings via public API
      AudioVolume->SetReverbSettings(ReverbSettings);

     TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
     Resp->SetBoolField(TEXT("success"), true);
     Resp->SetStringField(TEXT("actorName"), AudioVolume->GetName());
     TSharedPtr<FJsonObject> LocObj = McpHandlerUtils::CreateResultObject();
     LocObj->SetNumberField(TEXT("x"), Location.X);
     LocObj->SetNumberField(TEXT("y"), Location.Y);
     LocObj->SetNumberField(TEXT("z"), Location.Z);
     Resp->SetObjectField(TEXT("location"), LocObj);
     McpHandlerUtils::AddVerification(Resp, AudioVolume);
     Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                            TEXT("Reverb zone created"), Resp);
     return true;
   }
  return false;
}
#endif
}
