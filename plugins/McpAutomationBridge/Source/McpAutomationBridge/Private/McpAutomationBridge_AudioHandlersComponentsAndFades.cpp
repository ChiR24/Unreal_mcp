#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AudioHandlersPrivate.h"

namespace McpAudioHandlers
{
#if WITH_EDITOR
bool HandleComponentActions(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const FString& Lower,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
	if (Lower == TEXT("fade_sound_out") ||
		Lower == TEXT("fade_sound_in") ||
		Lower == TEXT("audio_fade_sound_out") ||
		Lower == TEXT("audio_fade_sound_in")) {
	FString ActorName;
	Payload->TryGetStringField(TEXT("actorName"), ActorName);
	FString ComponentName;
	Payload->TryGetStringField(TEXT("componentName"), ComponentName);
	double FadeTime = 1.0;
	Payload->TryGetNumberField(TEXT("fadeTime"), FadeTime);
	double TargetVol =
		(Lower == TEXT("fade_sound_in") || Lower == TEXT("audio_fade_sound_in"))
		? 1.0
		: 0.0;
	if (Lower == TEXT("fade_sound_in") || Lower == TEXT("audio_fade_sound_in"))
		Payload->TryGetNumberField(TEXT("targetVolume"), TargetVol);

	if (!GEditor)
	{
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
	if (TargetActor) {
		UAudioComponent *AudioComp = nullptr;

		// Search by component name if provided
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

		// Fall back to finding any AudioComponent via FindComponentByClass
		if (!AudioComp)
		{
			AudioComp = TargetActor->FindComponentByClass<UAudioComponent>();
		}

		// Fall back to iterating ALL components including transient/unregistered ones
		// SpawnSoundAttached creates components that may not appear in
		// FindComponentByClass results but are still owned by the actor
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

	// Final fallback: search all UAudioComponent instances for one owned by this actor
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

		if (AudioComp) {
			if (Lower == TEXT("fade_sound_in") ||
				Lower == TEXT("audio_fade_sound_in"))
				AudioComp->FadeIn((float)FadeTime, (float)TargetVol);
			else
				AudioComp->FadeOut((float)FadeTime, (float)TargetVol);

			TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
			Resp->SetBoolField(TEXT("success"), true);
			Resp->SetStringField(TEXT("actorName"), ActorName);
			Resp->SetStringField(TEXT("action"), Lower);
			McpHandlerUtils::AddVerification(Resp, TargetActor);
			Self->SendAutomationResponse(RequestingSocket, RequestId, true,
				TEXT("Sound fading"), Resp);
			return true;
		}
	}
	Self->SendAutomationError(RequestingSocket, RequestId,
		TEXT("Audio component not found on actor"),
		TEXT("COMPONENT_NOT_FOUND"));
	return true;
}

  // -------------------------------------------------------------------------
  // prime_sound
  // -------------------------------------------------------------------------
  // Pre-loads a sound asset for instant playback (reduces first-play latency).
  //
  // Payload:  { "soundPath": string }
  // Response: { "success": bool, "message": "Sound primed" }
  // -------------------------------------------------------------------------
  if (Lower == TEXT("prime_sound")) {
    FString SoundPath;
    Payload->TryGetStringField(TEXT("soundPath"), SoundPath);
    USoundBase *Sound = ResolveSoundAsset(SoundPath);
    if (!Sound) {
      Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Sound not found"),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    UGameplayStatics::PrimeSound(Sound);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Sound primed"), nullptr);
    return true;
  }

  // -------------------------------------------------------------------------
  // create_audio_component
  // -------------------------------------------------------------------------
  // Creates a UAudioComponent, optionally attached to an actor or at a location.
  //
  // Payload:  { "soundPath": string, "location"?: [x,y,z], "rotation"?: [p,y,r],
  //             "attachTo"?: string, "actorName"?: string,
  //             "volume"?: string, "pitch"?: string }
  // Response: { "success": bool, "componentPath": string,
  //             "componentName": string }
  // -------------------------------------------------------------------------
  if (Lower.StartsWith(TEXT("create_audio_component"))) {
    FString SoundPath;
    if (!Payload->TryGetStringField(TEXT("soundPath"), SoundPath))
      Payload->TryGetStringField(TEXT("path"), SoundPath);
    if (SoundPath.IsEmpty()) {
      Self->SendAutomationError(RequestingSocket, RequestId,
                          TEXT("soundPath required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    USoundBase *Sound = ResolveSoundAsset(SoundPath);
    if (!Sound) {
      Self->SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Sound asset not found: %s"), *SoundPath),
          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    FVector Location =
        ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);
    FRotator Rotation =
        ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    FString AttachTo;
    Payload->TryGetStringField(TEXT("attachTo"), AttachTo);
    if (AttachTo.IsEmpty())
      Payload->TryGetStringField(TEXT("actorName"), AttachTo);

    UAudioComponent *AudioComp = nullptr;
    UWorld *World =
        GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;

    if (!World) {
      Self->SendAutomationError(RequestingSocket, RequestId, TEXT("No editor world"),
                          TEXT("NO_WORLD"));
      return true;
    }

    if (!AttachTo.IsEmpty()) {
      AActor *ParentActor = FindAudioActorByName(AttachTo, World);
      if (ParentActor) {
        AudioComp = CreateRegisteredAudioComponent(ParentActor, Sound, Location, Rotation);
      } else {
        UE_LOG(LogMcpAudioHandlers, Warning,
               TEXT("create_audio_component: attachTo actor '%s' not found, "
                    "spawning at location."),
               *AttachTo);
      }
    }

    if (!AudioComp) {
      AudioComp = CreateAudioComponentAtEditorLocation(World, Sound, Location, Rotation, FString());
    }

    if (AudioComp) {
      FString VolumeStr;
      if (Payload->TryGetStringField(TEXT("volume"), VolumeStr))
        AudioComp->SetVolumeMultiplier(FCString::Atof(*VolumeStr));
      FString PitchStr;
      if (Payload->TryGetStringField(TEXT("pitch"), PitchStr))
        AudioComp->SetPitchMultiplier(FCString::Atof(*PitchStr));
      AudioComp->Activate(true);

      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("componentPath"), AudioComp->GetPathName());
      Resp->SetStringField(TEXT("componentName"), AudioComp->GetName());
      Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Audio component created"), Resp, FString());
      return true;
    }
    Self->SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create audio component"),
                        TEXT("CREATE_FAILED"));
    return true;
  }
  return false;
}
#endif
}
