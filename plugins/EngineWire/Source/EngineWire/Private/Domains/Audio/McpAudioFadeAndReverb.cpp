#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"
#include "EngineUtils.h"
#include "Domains/Audio/McpAudioPrivate.h"
#include "Domains/Volume/McpVolumeGeometry.h"
#include "Foundation/McpScopedEditorTransaction.h"

namespace McpAudioHandlers
{
// The audio components an actor owns. One spawned attached at runtime can be missing from the actor's own list,
// so the owner scan is the fallback.
static TArray<UAudioComponent*> AudioComponentsOf(AActor* Actor)
{
    TArray<UAudioComponent*> Found;
    Actor->GetComponents<UAudioComponent>(Found);
    if (Found.Num() == 0)
    {
        ForEachObjectOfClass(UAudioComponent::StaticClass(), [&Found, Actor](UObject* Obj)
        {
            UAudioComponent* Comp = Cast<UAudioComponent>(Obj);
            if (Comp && Comp->GetOwner() == Actor)
            {
                Found.Add(Comp);
            }
        }, true, RF_ClassDefaultObject);
    }
    return Found;
}

// The actor soundName names: an actor label or name, else the owner of an audio component of that name. A running
// game plays its own copies of the level's sounds, so its world is searched before the editor level.
static AActor* FindFadeActor(const FString& SoundName, FString& OutComponentName)
{
    for (UWorld* World : {GEditor->PlayWorld.Get(), GEditor->GetEditorWorldContext().World()})
    {
        if (!World)
        {
            continue;
        }
        if (AActor* Actor = FindAudioActorByName(SoundName, World))
        {
            return Actor;
        }
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            TArray<UAudioComponent*> Owned;
            It->GetComponents<UAudioComponent>(Owned);
            for (UAudioComponent* Comp : Owned)
            {
                if (Comp->GetName().Equals(SoundName, ESearchCase::IgnoreCase))
                {
                    OutComponentName = Comp->GetName();
                    return *It;
                }
            }
        }
    }
    return nullptr;
}

// fade_sound (fadeType FadeTo by default, or FadeIn/FadeOut), fade_sound_in and fade_sound_out: one lookup, one reply.
static void HandleFade(UEngineWireSubsystem* Self, const FString& RequestId, const FString& Lower,
                       const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const bool bInAction = Lower == TEXT("fade_sound_in");
    const bool bOutAction = Lower == TEXT("fade_sound_out");
    FString Mode = bInAction ? TEXT("FadeIn") : bOutAction ? TEXT("FadeOut") : TEXT("FadeTo");
    if (!bInAction && !bOutAction)
    {
        Payload->TryGetStringField(TEXT("fadeType"), Mode);
    }
    const bool bIn = Mode.Equals(TEXT("FadeIn"), ESearchCase::IgnoreCase);
    const bool bOut = Mode.Equals(TEXT("FadeOut"), ESearchCase::IgnoreCase);
    if (!bIn && !bOut && !Mode.Equals(TEXT("FadeTo"), ESearchCase::IgnoreCase))
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("fadeType '%s' is not FadeTo, FadeIn or FadeOut"), *Mode), TEXT("INVALID_ARGUMENT"));
        return;
    }
    // The in and out actions spell the duration fadeInTime and fadeOutTime; fadeTime is fade_sound's.
    double FadeTime = 1.0;
    Payload->TryGetNumberField(bInAction ? TEXT("fadeInTime") : bOutAction ? TEXT("fadeOutTime") : TEXT("fadeTime"), FadeTime);
    double TargetVolume = bIn ? 1.0 : 0.0;
    Payload->TryGetNumberField(TEXT("targetVolume"), TargetVolume);
    if (!FMath::IsFinite(FadeTime) || FadeTime < 0.0 || !FMath::IsFinite(TargetVolume) || TargetVolume < 0.0)
    {
        Self->SendAutomationError(Socket, RequestId,
            TEXT("The fade time and targetVolume must be finite and not negative"), TEXT("INVALID_ARGUMENT"));
        return;
    }

    FString SoundName, ComponentName, MatchedComponent;
    Payload->TryGetStringField(TEXT("soundName"), SoundName);
    Payload->TryGetStringField(TEXT("componentName"), ComponentName);
    AActor* Actor = GEditor && !SoundName.IsEmpty() ? FindFadeActor(SoundName, MatchedComponent) : nullptr;
    if (!Actor)
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("No actor or audio component named '%s' (soundName takes an actor label or name, or an audio component name)"), *SoundName),
            TEXT("ACTOR_NOT_FOUND"));
        return;
    }
    if (ComponentName.IsEmpty())
    {
        ComponentName = MatchedComponent;
    }
    const TArray<UAudioComponent*> Components = AudioComponentsOf(Actor);
    UAudioComponent* AudioComp = ComponentName.IsEmpty() && Components.Num() > 0 ? Components[0] : nullptr;
    TArray<FString> Names;
    for (UAudioComponent* Comp : Components)
    {
        Names.Add(Comp->GetName());
        if (!ComponentName.IsEmpty() && Comp->GetName().Equals(ComponentName, ESearchCase::IgnoreCase))
        {
            AudioComp = Comp;
        }
    }
    // A componentName that matched nothing used to fade the actor's first audio component instead.
    if (!AudioComp)
    {
        Self->SendAutomationError(Socket, RequestId, Components.Num() == 0
            ? FString::Printf(TEXT("%s has no audio component"), *McpActorRef(Actor))
            : FString::Printf(TEXT("No audio component '%s' on %s; it has: %s"), *ComponentName, *McpActorRef(Actor), *FString::Join(Names, TEXT(", "))),
            TEXT("COMPONENT_NOT_FOUND"));
        return;
    }

    // The engine ignores a fade of a component that is not playing: a volume change has nothing to change, and a fade
    // out has nothing left to do.
    const bool bWasPlaying = AudioComp->IsPlaying();
    if (!bIn && !bOut && !bWasPlaying)
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("%s is not playing, so there is no volume to change; a fade in starts it at a volume"), *AudioComp->GetName()),
            TEXT("SOUND_NOT_PLAYING"));
        return;
    }
    if (bIn)
    {
        AudioComp->FadeIn((float)FadeTime, (float)TargetVolume);
    }
    else if (bOut)
    {
        AudioComp->FadeOut((float)FadeTime, (float)TargetVolume);
    }
    else
    {
        // FadeIn here restarted the playing sound from its beginning; this changes the volume of the one playing.
        AudioComp->AdjustVolume((float)FadeTime, (float)TargetVolume);
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("actorName"), McpActorRef(Actor));
    Resp->SetStringField(TEXT("componentName"), AudioComp->GetName());
    Resp->SetNumberField(TEXT("fadeTime"), FadeTime);
    Resp->SetNumberField(TEXT("targetVolume"), TargetVolume);
    Resp->SetBoolField(TEXT("playing"), AudioComp->IsPlaying());
    Resp->SetStringField(TEXT("action"), Lower);
    McpHandlerUtils::AddVerification(Resp, Actor);
    const TCHAR* Message = bIn ? (bWasPlaying ? TEXT("Sound restarted from its beginning, fading in") : TEXT("Sound fading in"))
        : bOut ? (bWasPlaying ? TEXT("Sound fading out; it stops when the fade ends") : TEXT("Sound was not playing; nothing to fade out"))
        : TargetVolume <= 0.0 ? TEXT("Sound fading to silence; it stops when the fade ends") : TEXT("Sound fading to the target volume");
    Self->SendAutomationResponse(Socket, RequestId, true, Message, Resp);
}

bool HandleFadeAndReverbActions(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const FString& Lower,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
   if (Lower == TEXT("fade_sound") || Lower == TEXT("fade_sound_in") || Lower == TEXT("fade_sound_out")) {
     HandleFade(Self, RequestId, Lower, Payload, RequestingSocket);
     return true;
   }

   if (Lower == TEXT("create_reverb_zone")) {
     FString ZoneName;
     if (!Payload->TryGetStringField(TEXT("name"), ZoneName) || ZoneName.IsEmpty()) {
       Self->SendAutomationError(RequestingSocket, RequestId,
                           TEXT("name required"), TEXT("INVALID_ARGUMENT"));
       return true;
     }

     // Either vector spelling; both were read as arrays only.
     const FVector Location = ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);
     const FVector Size = ExtractVectorField(Payload, TEXT("size"), FVector(500.0f, 500.0f, 500.0f));
     if (Size.X <= 0.0 || Size.Y <= 0.0 || Size.Z <= 0.0) {
       Self->SendAutomationError(RequestingSocket, RequestId,
                           TEXT("size must be positive on every axis"), TEXT("INVALID_ARGUMENT"));
       return true;
     }

     FString ReverbEffectPath;
     Payload->TryGetStringField(TEXT("reverbEffect"), ReverbEffectPath);
     // Loaded before the spawn: a wrong path used to leave a zone with no reverb and report success.
     UReverbEffect *ReverbEffect = ReverbEffectPath.IsEmpty() ? nullptr
         : LoadObject<UReverbEffect>(nullptr, *AudioObjectPath(ReverbEffectPath), nullptr, LOAD_NoWarn);
     if (!ReverbEffectPath.IsEmpty() && !ReverbEffect) {
       Self->SendAutomationError(RequestingSocket, RequestId,
           FString::Printf(TEXT("ReverbEffect not found: %s"), *ReverbEffectPath), TEXT("ASSET_NOT_FOUND"));
       return true;
     }
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

      // Spawn AudioVolume actor, as one undo step with its brush and reverb settings.
      FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Create Reverb Zone")), EMcpMutationDurability::EditorStateOnly,
                                              TArray<UObject*>());
      FActorSpawnParameters SpawnParams;
      SpawnParams.Name = FName(*ZoneName);
      SpawnParams.ObjectFlags = RF_Transactional;
      AAudioVolume *AudioVolume = World->SpawnActor<AAudioVolume>(Location, FRotator::ZeroRotator, SpawnParams);
     if (!AudioVolume) {
       Self->SendAutomationError(RequestingSocket, RequestId,
                           TEXT("Failed to spawn AudioVolume"), TEXT("SPAWN_FAILED"));
       return true;
     }

     // Set actor label
     AudioVolume->SetActorLabel(ZoneName);

      // A spawned volume has no brush, so the zone enclosed nothing and size was never used:
      // build the box the way the volume tools do (the extent is half the size).
      VolumeHelpers::CreateBoxBrushForVolume(AudioVolume, Size * 0.5f);

      // Create reverb settings and apply via public API
      FReverbSettings ReverbSettings;
      ReverbSettings.bApplyReverb = true;

      ReverbSettings.ReverbEffect = ReverbEffect;

      // Set volume settings
      ReverbSettings.Volume = (float)Volume;
      ReverbSettings.FadeTime = (float)FadeTime;

      // Apply settings via public API
      AudioVolume->SetReverbSettings(ReverbSettings);

     TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
     Resp->SetBoolField(TEXT("success"), true);
     Resp->SetStringField(TEXT("actorName"), AudioVolume->GetName());
     Resp->SetObjectField(TEXT("location"), McpHandlerUtils::VectorToJson(Location));
     McpHandlerUtils::AddVerification(Resp, AudioVolume);
     Transaction.DescribeInto(Resp);
     Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                            TEXT("Reverb zone created"), Resp);
     return true;
   }
  return false;
}
}
