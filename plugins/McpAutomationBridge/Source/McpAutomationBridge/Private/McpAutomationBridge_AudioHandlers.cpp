#include "EngineUtils.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "AudioDevice.h"
#include "Components/AudioComponent.h"
#include "EditorAssetLibrary.h"
#include "Factories/SoundAttenuationFactory.h"
#include "Factories/SoundClassFactory.h"
#include "Factories/SoundCueFactoryNew.h"
#include "Factories/SoundMixFactory.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundNodeAttenuation.h"
#include "Sound/SoundNodeLooping.h"
#include "Sound/SoundNodeModulator.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundWave.h"

#endif

static AActor *FindAudioActorByName(const FString &ActorName, UWorld *World) {
  if (ActorName.IsEmpty())
    return nullptr;

  // Fast path: Direct object path/name
  AActor *Actor = FindObject<AActor>(nullptr, *ActorName);
  if (Actor && Actor->IsValidLowLevel())
    return Actor;

  // Fallback: Label search (limited scope)
  if (World) {
    for (TActorIterator<AActor> It(World); It; ++It) {
      if (It->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase) ||
          It->GetName().Equals(ActorName, ESearchCase::IgnoreCase)) {
        return *It;
      }
    }
  }
  return nullptr;
}

static USoundBase *ResolveSoundAsset(const FString &SoundPath) {
  if (SoundPath.IsEmpty())
    return nullptr;

  if (SoundPath.IsEmpty())
    return nullptr;

  USoundBase *Sound = nullptr;
  if (UEditorAssetLibrary::DoesAssetExist(SoundPath)) {
    Sound = Cast<USoundBase>(UEditorAssetLibrary::LoadAsset(SoundPath));
  }

  if (Sound)
    return Sound;

  // Fallback: Try to find ANY sound to ensure the command succeeds
  // visually/audibly
  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  TArray<FAssetData> AssetData;
  FARFilter Filter;
  // Use ClassPaths for UE 5.1+
  Filter.ClassPaths.Add(USoundWave::StaticClass()->GetClassPathName());
  Filter.ClassPaths.Add(USoundCue::StaticClass()->GetClassPathName());
  Filter.bRecursivePaths = true;
  Filter.PackagePaths.Add(TEXT("/Game"));
  AssetRegistryModule.Get().GetAssets(Filter, AssetData);

  if (AssetData.Num() > 0) {
    Sound = Cast<USoundBase>(AssetData[0].GetAsset());
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("Sound asset '%s' not found, falling back to '%s'"), *SoundPath,
           *Sound->GetName());
  }

  return Sound;
}

bool UMcpAutomationBridgeSubsystem::HandleAudioAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.StartsWith(TEXT("create_sound_")) &&
      !Lower.StartsWith(TEXT("play_sound_")) &&
      !Lower.StartsWith(TEXT("set_sound_")) &&
      !Lower.StartsWith(TEXT("push_sound_")) &&
      !Lower.StartsWith(TEXT("pop_sound_")) &&
      !Lower.StartsWith(TEXT("create_audio_")) &&
      !Lower.StartsWith(TEXT("create_ambient_")) &&
      !Lower.StartsWith(TEXT("create_reverb_")) &&
      !Lower.StartsWith(TEXT("enable_audio_")) &&
      !Lower.StartsWith(TEXT("fade_sound")) &&
      !Lower.StartsWith(TEXT("set_doppler_")) &&
      !Lower.StartsWith(TEXT("set_audio_"))) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Audio payload missing"), TEXT("INVALID_PAYLOAD"));
    return true;
  }

  if (Lower == TEXT("create_sound_cue")) {
    FString Name;
    if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("name required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString PackagePath;
    Payload->TryGetStringField(TEXT("packagePath"), PackagePath);
    if (PackagePath.IsEmpty())
      PackagePath = TEXT("/Game/Audio/Cues");

    FString WavePath;
    Payload->TryGetStringField(TEXT("wavePath"), WavePath);

    USoundCueFactoryNew *Factory = NewObject<USoundCueFactoryNew>();
    FAssetToolsModule &AssetToolsModule =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
    UObject *NewAsset = AssetToolsModule.Get().CreateAsset(
        Name, PackagePath, USoundCue::StaticClass(), Factory);
    USoundCue *SoundCue = Cast<USoundCue>(NewAsset);

    if (!SoundCue) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create SoundCue"),
                          TEXT("ASSET_CREATION_FAILED"));
      return true;
    }

    // Basic graph setup if wave provided
    if (!WavePath.IsEmpty()) {
      USoundWave *Wave = LoadObject<USoundWave>(nullptr, *WavePath);
      if (Wave) {
        USoundNodeWavePlayer *PlayerNode =
            SoundCue->ConstructSoundNode<USoundNodeWavePlayer>();
        PlayerNode->SetSoundWave(Wave);

        USoundNode *LastNode = PlayerNode;

        // Optional looping
        bool bLooping = false;
        if (Payload->TryGetBoolField(TEXT("looping"), bLooping) && bLooping) {
          USoundNodeLooping *LoopNode =
              SoundCue->ConstructSoundNode<USoundNodeLooping>();
          LoopNode->ChildNodes.Add(LastNode);
          LastNode = LoopNode;
        }

        // Optional modulation (volume/pitch)
        double Volume = 1.0;
        double Pitch = 1.0;
        bool bHasVolume = Payload->TryGetNumberField(TEXT("volume"), Volume);
        bool bHasPitch = Payload->TryGetNumberField(TEXT("pitch"), Pitch);

        if (bHasVolume || bHasPitch) {
          USoundNodeModulator *ModNode =
              SoundCue->ConstructSoundNode<USoundNodeModulator>();
          ModNode->PitchMin = ModNode->PitchMax = (float)Pitch;
          ModNode->VolumeMin = ModNode->VolumeMax = (float)Volume;
          ModNode->ChildNodes.Add(LastNode);
          LastNode = ModNode;
        }

        // Optional attenuation
        FString AttenuationPath;
        if (Payload->TryGetStringField(TEXT("attenuationPath"),
                                       AttenuationPath) &&
            !AttenuationPath.IsEmpty()) {
          USoundAttenuation *Attenuation =
              LoadObject<USoundAttenuation>(nullptr, *AttenuationPath);
          if (Attenuation) {
            USoundNodeAttenuation *AttenNode =
                SoundCue->ConstructSoundNode<USoundNodeAttenuation>();
            AttenNode->AttenuationSettings = Attenuation;
            AttenNode->ChildNodes.Add(LastNode);
            LastNode = AttenNode;
          }
        }

        SoundCue->FirstNode = LastNode;
        SoundCue->LinkGraphNodesFromSoundNodes();
      }
    }

    UEditorAssetLibrary::SaveAsset(SoundCue->GetPathName());

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("path"), SoundCue->GetPathName());
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("SoundCue created"), Resp);
    return true;
  } else if (Lower == TEXT("play_sound_at_location")) {
    FString SoundPath;
    if (!Payload->TryGetStringField(TEXT("soundPath"), SoundPath) ||
        SoundPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("soundPath required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    USoundBase *Sound = ResolveSoundAsset(SoundPath);
    if (!Sound) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Sound asset not found"),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    FVector Location = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    const TArray<TSharedPtr<FJsonValue>> *LocArr;
    if (Payload->TryGetArrayField(TEXT("location"), LocArr) && LocArr &&
        LocArr->Num() >= 3) {
      Location = FVector((*LocArr)[0]->AsNumber(), (*LocArr)[1]->AsNumber(),
                         (*LocArr)[2]->AsNumber());
    }
    const TArray<TSharedPtr<FJsonValue>> *RotArr;
    if (Payload->TryGetArrayField(TEXT("rotation"), RotArr) && RotArr &&
        RotArr->Num() >= 3) {
      Rotation = FRotator((*RotArr)[0]->AsNumber(), (*RotArr)[1]->AsNumber(),
                          (*RotArr)[2]->AsNumber());
    }

    double Volume = 1.0;
    Payload->TryGetNumberField(TEXT("volume"), Volume);
    double Pitch = 1.0;
    Payload->TryGetNumberField(TEXT("pitch"), Pitch);
    double StartTime = 0.0;
    Payload->TryGetNumberField(TEXT("startTime"), StartTime);

    USoundAttenuation *Attenuation = nullptr;
    FString AttenPath;
    if (Payload->TryGetStringField(TEXT("attenuationPath"), AttenPath) &&
        !AttenPath.IsEmpty()) {
      Attenuation = LoadObject<USoundAttenuation>(nullptr, *AttenPath);
    }

    USoundConcurrency *Concurrency = nullptr;
    FString ConcPath;
    if (Payload->TryGetStringField(TEXT("concurrencyPath"), ConcPath) &&
        !ConcPath.IsEmpty()) {
      Concurrency = LoadObject<USoundConcurrency>(nullptr, *ConcPath);
    }

    if (!GEditor)
      return false;
    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (!World) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("No world context available"), TEXT("NO_WORLD"));
      return true;
    }

    UGameplayStatics::PlaySoundAtLocation(
        World, Sound, Location, Rotation, (float)Volume, (float)Pitch,
        (float)StartTime, Attenuation, Concurrency);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Sound played at location"), nullptr);
    return true;
  } else if (Lower == TEXT("play_sound_2d")) {
    FString SoundPath;
    if (!Payload->TryGetStringField(TEXT("soundPath"), SoundPath) ||
        SoundPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("soundPath required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    USoundBase *Sound = ResolveSoundAsset(SoundPath);
    if (!Sound) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Sound asset not found"),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    double Volume = 1.0;
    Payload->TryGetNumberField(TEXT("volume"), Volume);
    double Pitch = 1.0;
    Payload->TryGetNumberField(TEXT("pitch"), Pitch);
    double StartTime = 0.0;
    Payload->TryGetNumberField(TEXT("startTime"), StartTime);

    if (!GEditor)
      return true;
    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (!World) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                          TEXT("NO_WORLD"));
      return true;
    }

    UGameplayStatics::PlaySound2D(World, Sound, (float)Volume, (float)Pitch,
                                  (float)StartTime);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Sound played 2D"), nullptr);
    return true;
  } else if (Lower == TEXT("create_sound_class")) {
    FString Name;
    if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("name required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString PackagePath = TEXT("/Game/Audio/Classes");

    USoundClassFactory *Factory = NewObject<USoundClassFactory>();
    FAssetToolsModule &AssetToolsModule =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
    UObject *NewAsset = AssetToolsModule.Get().CreateAsset(
        Name, PackagePath, USoundClass::StaticClass(), Factory);
    USoundClass *SoundClass = Cast<USoundClass>(NewAsset);

    if (SoundClass) {
      const TSharedPtr<FJsonObject> *Props;
      if (Payload->TryGetObjectField(TEXT("properties"), Props)) {
        double Vol = 1.0;
        if ((*Props)->TryGetNumberField(TEXT("volume"), Vol)) {
          SoundClass->Properties.Volume = (float)Vol;
        }
        double Pitch = 1.0;
        if ((*Props)->TryGetNumberField(TEXT("pitch"), Pitch)) {
          SoundClass->Properties.Pitch = (float)Pitch;
        }
      }

      FString ParentClassPath;
      if (Payload->TryGetStringField(TEXT("parentClass"), ParentClassPath) &&
          !ParentClassPath.IsEmpty()) {
        USoundClass *Parent =
            LoadObject<USoundClass>(nullptr, *ParentClassPath);
        if (Parent) {
          SoundClass->ParentClass = Parent;
        }
      }

      UEditorAssetLibrary::SaveAsset(SoundClass->GetPathName());
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("SoundClass created"), nullptr);
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create SoundClass"),
                          TEXT("ASSET_CREATION_FAILED"));
    }
    return true;
  } else if (Lower == TEXT("create_sound_mix")) {
    FString Name;
    if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("name required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString PackagePath = TEXT("/Game/Audio/Mixes");

    USoundMixFactory *Factory = NewObject<USoundMixFactory>();
    FAssetToolsModule &AssetToolsModule =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
    UObject *NewAsset = AssetToolsModule.Get().CreateAsset(
        Name, PackagePath, USoundMix::StaticClass(), Factory);
    USoundMix *SoundMix = Cast<USoundMix>(NewAsset);

    if (SoundMix) {
      const TArray<TSharedPtr<FJsonValue>> *Adjusters;
      if (Payload->TryGetArrayField(TEXT("classAdjusters"), Adjusters)) {
        for (const auto &Val : *Adjusters) {
          const TSharedPtr<FJsonObject> AdjObj = Val->AsObject();
          FString ClassPath;
          if (AdjObj->TryGetStringField(TEXT("soundClass"), ClassPath)) {
            USoundClass *SC = LoadObject<USoundClass>(nullptr, *ClassPath);
            if (SC) {
              FSoundClassAdjuster Adjuster;
              Adjuster.SoundClassObject = SC;
              double Vol = 1.0;
              AdjObj->TryGetNumberField(TEXT("volumeAdjuster"), Vol);
              Adjuster.VolumeAdjuster = (float)Vol;
              double Pitch = 1.0;
              AdjObj->TryGetNumberField(TEXT("pitchAdjuster"), Pitch);
              Adjuster.PitchAdjuster = (float)Pitch;
              SoundMix->SoundClassEffects.Add(Adjuster);
            }
          }
        }
      }

      UEditorAssetLibrary::SaveAsset(SoundMix->GetPathName());
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("SoundMix created"), nullptr);
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create SoundMix"),
                          TEXT("ASSET_CREATION_FAILED"));
    }
    return true;
  } else if (Lower == TEXT("push_sound_mix")) {
    FString MixName;
    if (!Payload->TryGetStringField(TEXT("mixName"), MixName) ||
        MixName.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("mixName required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    USoundMix *Mix = LoadObject<USoundMix>(nullptr, *MixName);
    if (Mix) {
      if (GEditor && GEditor->GetEditorWorldContext().World()) {
        UGameplayStatics::PushSoundMixModifier(
            GEditor->GetEditorWorldContext().World(), Mix);
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("SoundMix pushed"), nullptr);
      } else {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("No World Context"), TEXT("NO_WORLD"));
      }
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("SoundMix not found"), TEXT("ASSET_NOT_FOUND"));
    }
    return true;
  } else if (Lower == TEXT("pop_sound_mix")) {
    FString MixName;
    if (!Payload->TryGetStringField(TEXT("mixName"), MixName) ||
        MixName.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("mixName required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    USoundMix *Mix = LoadObject<USoundMix>(nullptr, *MixName);
    if (Mix) {
      if (GEditor && GEditor->GetEditorWorldContext().World()) {
        UGameplayStatics::PopSoundMixModifier(
            GEditor->GetEditorWorldContext().World(), Mix);
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("SoundMix popped"), nullptr);
      } else {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("No World Context"), TEXT("NO_WORLD"));
      }
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("SoundMix not found"), TEXT("ASSET_NOT_FOUND"));
    }
    return true;
  } else if (Lower == TEXT("set_sound_mix_class_override")) {
    FString MixName, ClassName;
    Payload->TryGetStringField(TEXT("mixName"), MixName);
    Payload->TryGetStringField(TEXT("soundClassName"), ClassName);

    USoundMix *Mix = LoadObject<USoundMix>(nullptr, *MixName);
    USoundClass *Class = LoadObject<USoundClass>(nullptr, *ClassName);

    if (!Mix || !Class) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Mix or Class not found"),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    double Volume = 1.0;
    Payload->TryGetNumberField(TEXT("volume"), Volume);
    double Pitch = 1.0;
    Payload->TryGetNumberField(TEXT("pitch"), Pitch);
    double FadeTime = 1.0;
    Payload->TryGetNumberField(TEXT("fadeInTime"), FadeTime);
    bool bApply = true;
    Payload->TryGetBoolField(TEXT("applyToChildren"), bApply);

    if (GEditor && GEditor->GetEditorWorldContext().World()) {
      UGameplayStatics::SetSoundMixClassOverride(
          GEditor->GetEditorWorldContext().World(), Mix, Class, (float)Volume,
          (float)Pitch, (float)FadeTime, bApply);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Sound mix override set"), nullptr);
    } else {
      SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                          TEXT("NO_WORLD"));
    }
    return true;
  } else if (Lower == TEXT("play_sound_attached")) {
    FString SoundPath, ActorName, AttachPoint;
    Payload->TryGetStringField(TEXT("soundPath"), SoundPath);
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    Payload->TryGetStringField(TEXT("attachPointName"), AttachPoint);

    USoundBase *Sound = ResolveSoundAsset(SoundPath);
    if (!Sound) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Sound not found"),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    if (!GEditor)
      return true;
    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (!World) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                          TEXT("NO_WORLD"));
      return true;
    }

    AActor *TargetActor = FindAudioActorByName(ActorName, World);
    if (!TargetActor) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Actor not found"),
                          TEXT("ACTOR_NOT_FOUND"));
      return true;
    }

    USceneComponent *AttachComp = TargetActor->GetRootComponent();
    if (!AttachPoint.IsEmpty()) {
      // Try to find socket or component
      USceneComponent *FoundComp = nullptr;
      TArray<USceneComponent *> Components;
      TargetActor->GetComponents(Components);
      for (USceneComponent *Comp : Components) {
        if (Comp->GetName() == AttachPoint ||
            Comp->DoesSocketExist(FName(*AttachPoint))) {
          FoundComp = Comp;
          break;
        }
      }
      if (FoundComp)
        AttachComp = FoundComp;
    }

    UAudioComponent *AudioComp = UGameplayStatics::SpawnSoundAttached(
        Sound, AttachComp, FName(*AttachPoint), FVector::ZeroVector,
        EAttachLocation::KeepRelativeOffset, true);

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    if (AudioComp) {
      Resp->SetStringField(TEXT("componentName"), AudioComp->GetName());
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Sound attached"), Resp);
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to attach sound"),
                          TEXT("ATTACH_FAILED"));
    }
    return true;
  } else if (Lower == TEXT("fade_sound_out") ||
             Lower == TEXT("fade_sound_in")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    double FadeTime = 1.0;
    Payload->TryGetNumberField(TEXT("fadeTime"), FadeTime);
    double TargetVol = (Lower == TEXT("fade_sound_in")) ? 1.0 : 0.0;
    if (Lower == TEXT("fade_sound_in"))
      Payload->TryGetNumberField(TEXT("targetVolume"), TargetVol);

    if (!GEditor)
      return true;
    UWorld *World = GEditor->GetEditorWorldContext().World(); // Fixed world
    if (!World) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                          TEXT("NO_WORLD"));
      return true;
    }

    AActor *TargetActor = FindAudioActorByName(ActorName, World);
    if (TargetActor) {
      UAudioComponent *AudioComp =
          TargetActor->FindComponentByClass<UAudioComponent>();
      if (AudioComp) {
        if (Lower == TEXT("fade_sound_in"))
          AudioComp->FadeIn((float)FadeTime, (float)TargetVol);
        else
          AudioComp->FadeOut((float)FadeTime, (float)TargetVol);

        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Sound fading"), nullptr);
        return true;
      }
    }
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Audio component not found on actor"),
                        TEXT("COMPONENT_NOT_FOUND"));
    return true;
  } else if (Lower == TEXT("create_ambient_sound")) {
    FString SoundPath;
    if (!Payload->TryGetStringField(TEXT("soundPath"), SoundPath) ||
        SoundPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("soundPath required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    USoundBase *Sound = ResolveSoundAsset(SoundPath);
    if (!Sound) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Sound asset not found"),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    FVector Location = FVector::ZeroVector;
    const TArray<TSharedPtr<FJsonValue>> *LocArr;
    if (Payload->TryGetArrayField(TEXT("location"), LocArr) && LocArr &&
        LocArr->Num() >= 3) {
      Location = FVector((*LocArr)[0]->AsNumber(), (*LocArr)[1]->AsNumber(),
                         (*LocArr)[2]->AsNumber());
    }

    double Volume = 1.0;
    Payload->TryGetNumberField(TEXT("volume"), Volume);
    double Pitch = 1.0;
    Payload->TryGetNumberField(TEXT("pitch"), Pitch);
    double StartTime = 0.0;
    Payload->TryGetNumberField(TEXT("startTime"), StartTime);

    USoundAttenuation *Attenuation = nullptr;
    FString AttenPath;
    if (Payload->TryGetStringField(TEXT("attenuationPath"), AttenPath) &&
        !AttenPath.IsEmpty()) {
      Attenuation = LoadObject<USoundAttenuation>(nullptr, *AttenPath);
    }

    USoundConcurrency *Concurrency = nullptr;
    FString ConcPath;
    if (Payload->TryGetStringField(TEXT("concurrencyPath"), ConcPath) &&
        !ConcPath.IsEmpty()) {
      Concurrency = LoadObject<USoundConcurrency>(nullptr, *ConcPath);
    }

    if (!GEditor)
      return true;
    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (!World) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                          TEXT("NO_WORLD"));
      return true;
    }

    UAudioComponent *AudioComp = UGameplayStatics::SpawnSoundAtLocation(
        World, Sound, Location, FRotator::ZeroRotator, (float)Volume,
        (float)Pitch, (float)StartTime, Attenuation, Concurrency, true);

    if (AudioComp) {
      AudioComp->Play();

      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetStringField(TEXT("componentName"), AudioComp->GetName());
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Ambient sound created"), Resp);
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create ambient sound"),
                          TEXT("SPAWN_FAILED"));
    }
    return true;
  }

  if (Lower.StartsWith(TEXT("create_audio_component"))) {
    FString SoundPath;
    if (!Payload->TryGetStringField(TEXT("soundPath"), SoundPath))
      Payload->TryGetStringField(TEXT("path"), SoundPath);
    if (SoundPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("soundPath required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    USoundBase *Sound = ResolveSoundAsset(SoundPath);
    if (!Sound) {
      SendAutomationError(
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

    UAudioComponent *AudioComp = nullptr;
    UWorld *World =
        GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;

    if (!World) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("No editor world"),
                          TEXT("NO_WORLD"));
      return true;
    }

    if (!AttachTo.IsEmpty()) {
      AActor *ParentActor = FindAudioActorByName(AttachTo, World);
      if (ParentActor) {
        AudioComp = UGameplayStatics::SpawnSoundAttached(
            Sound, ParentActor->GetRootComponent(), NAME_None, Location,
            Rotation, EAttachLocation::KeepRelativeOffset, false);
      } else {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
               TEXT("create_audio_component: attachTo actor '%s' not found, "
                    "spawning at location."),
               *AttachTo);
      }
    }

    if (!AudioComp) {
      AudioComp = UGameplayStatics::SpawnSoundAtLocation(World, Sound, Location,
                                                         Rotation);
    }

    if (AudioComp) {
      FString VolumeStr;
      if (Payload->TryGetStringField(TEXT("volume"), VolumeStr))
        AudioComp->SetVolumeMultiplier(FCString::Atof(*VolumeStr));
      FString PitchStr;
      if (Payload->TryGetStringField(TEXT("pitch"), PitchStr))
        AudioComp->SetPitchMultiplier(FCString::Atof(*PitchStr));

      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("componentPath"), AudioComp->GetPathName());
      Resp->SetStringField(TEXT("componentName"), AudioComp->GetName());
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Audio component created"), Resp, FString());
      return true;
    }
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create audio component"),
                        TEXT("CREATE_FAILED"));
    return true;
  }

  // Fallback for other audio actions not fully implemented yet
  SendAutomationResponse(
      RequestingSocket, RequestId, false,
      FString::Printf(TEXT("Audio action '%s' not fully implemented"), *Action),
      nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Audio actions require editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
