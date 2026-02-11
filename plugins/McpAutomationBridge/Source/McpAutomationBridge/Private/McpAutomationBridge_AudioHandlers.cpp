#include "EngineUtils.h"
#include "Dom/JsonObject.h"
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
#include "Sound/DialogueVoice.h"
#include "Sound/DialogueWave.h"
#include "Sound/ReverbEffect.h"
#include "Sound/SoundEffectSubmix.h"

#endif

/**
 * Finds an actor by object path/name or by actor label/name within an optional world.
 *
 * Searches first for an exact object path or registered name, and if not found and a World is provided,
 * iterates actors in that World comparing actor label and actor name case-insensitively.
 *
 * @param ActorName Actor object path, registered name, or actor label to search for.
 * @param World Optional world to search actor labels/names in when direct lookup fails.
 * @return `AActor*` Pointer to the matched actor, `nullptr` if no matching actor is found or ActorName is empty.
 */
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

/**
 * @brief Resolves a USoundBase asset from an asset path or an asset name.
 *
 * Attempts to load the sound by the provided path; if the input appears to be a simple name
 * (no path separators), searches the project's /Game assets for a matching USoundWave or
 * USoundCue by name.
 *
 * @param SoundPath Asset path (e.g. "/Game/Audio/MyCue.MyCue") or an asset name (e.g. "MyCue").
 * @return USoundBase* Pointer to the resolved sound asset, or nullptr if not found.
 */
static USoundBase *ResolveSoundAsset(const FString &SoundPath) {
  if (SoundPath.IsEmpty())
    return nullptr;

  USoundBase *Sound = nullptr;
  if (UEditorAssetLibrary::DoesAssetExist(SoundPath)) {
    Sound = Cast<USoundBase>(UEditorAssetLibrary::LoadAsset(SoundPath));
  }

  if (Sound)
    return Sound;

  // Optimization: If it looks like a path and wasn't found, fail immediately
  if (SoundPath.Contains(TEXT("/"))) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("Sound asset '%s' not found (skipping recursive search)."),
           *SoundPath);
    return nullptr;
  }

  // Fallback: Try to find the asset by Name
  FString AssetName = FPaths::GetBaseFilename(SoundPath);
  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  TArray<FAssetData> AssetData;
  FARFilter Filter;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  Filter.ClassPaths.Add(USoundWave::StaticClass()->GetClassPathName());
  Filter.ClassPaths.Add(USoundCue::StaticClass()->GetClassPathName());
#else
  // UE 5.0 fallback - use ClassNames instead of ClassPaths
  Filter.ClassNames.Add(USoundWave::StaticClass()->GetFName());
  Filter.ClassNames.Add(USoundCue::StaticClass()->GetFName());
#endif
  Filter.bRecursivePaths = true;
  Filter.PackagePaths.Add(TEXT("/Game"));
  AssetRegistryModule.Get().GetAssets(Filter, AssetData);

  for (const FAssetData &Data : AssetData) {
    if (Data.AssetName.ToString().Equals(AssetName, ESearchCase::IgnoreCase)) {
      Sound = Cast<USoundBase>(Data.GetAsset());
      if (Sound) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
               TEXT("Resolved sound '%s' to '%s'"), *SoundPath,
               *Sound->GetPathName());
        return Sound;
      }
    }
  }

  UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
         TEXT("Sound asset '%s' not found."), *SoundPath);
  return nullptr;
}

/**
 * @brief Resolve a USoundMix by asset path or asset name.
 *
 * Attempts to load a USoundMix using the provided MixPath. If MixPath contains a
 * full asset path and the asset exists, that asset is returned. If MixPath does
 * not contain a path separator, the function treats it as an asset name and
 * searches the /Game packages for a matching USoundMix (case-insensitive).
 *
 * @param MixPath Asset path or asset name to resolve.
 * @return USoundMix* Pointer to the resolved USoundMix, or nullptr if not found.
 */
static USoundMix *ResolveSoundMix(const FString &MixPath) {
  if (MixPath.IsEmpty())
    return nullptr;

  USoundMix *Mix = nullptr;
  if (UEditorAssetLibrary::DoesAssetExist(MixPath)) {
    Mix = Cast<USoundMix>(UEditorAssetLibrary::LoadAsset(MixPath));
  }
  if (Mix)
    return Mix;

  if (MixPath.Contains(TEXT("/")))
    return nullptr;

  FString AssetName = FPaths::GetBaseFilename(MixPath);
  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  TArray<FAssetData> AssetData;
  FARFilter Filter;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  Filter.ClassPaths.Add(USoundMix::StaticClass()->GetClassPathName());
#else
  Filter.ClassNames.Add(USoundMix::StaticClass()->GetFName());
#endif
  Filter.bRecursivePaths = true;
  Filter.PackagePaths.Add(TEXT("/Game"));
  AssetRegistryModule.Get().GetAssets(Filter, AssetData);

  for (const FAssetData &Data : AssetData) {
    if (Data.AssetName.ToString().Equals(AssetName, ESearchCase::IgnoreCase)) {
      Mix = Cast<USoundMix>(Data.GetAsset());
      if (Mix)
        return Mix;
    }
  }
  return nullptr;
}

/**
 * @brief Locates and returns a USoundClass by asset path or by asset name.
 *
 * Attempts to load the sound class directly if ClassPath refers to an existing asset; otherwise,
 * if ClassPath does not contain a '/' it searches the project's /Game assets for a sound class
 * with a matching name (case-insensitive).
 *
 * @param ClassPath Asset path (e.g. "/Game/Audio/MyClass") or asset name ("MyClass").
 * @return USoundClass* Pointer to the resolved sound class, or nullptr if not found or ClassPath is empty.
 */
static USoundClass *ResolveSoundClass(const FString &ClassPath) {
  if (ClassPath.IsEmpty())
    return nullptr;

  USoundClass *Class = nullptr;
  if (UEditorAssetLibrary::DoesAssetExist(ClassPath)) {
    Class = Cast<USoundClass>(UEditorAssetLibrary::LoadAsset(ClassPath));
  }
  if (Class)
    return Class;

  if (ClassPath.Contains(TEXT("/")))
    return nullptr;

  FString AssetName = FPaths::GetBaseFilename(ClassPath);
  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  TArray<FAssetData> AssetData;
  FARFilter Filter;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  Filter.ClassPaths.Add(USoundClass::StaticClass()->GetClassPathName());
#else
  Filter.ClassNames.Add(USoundClass::StaticClass()->GetFName());
#endif
  Filter.bRecursivePaths = true;
  Filter.PackagePaths.Add(TEXT("/Game"));
  AssetRegistryModule.Get().GetAssets(Filter, AssetData);

  for (const FAssetData &Data : AssetData) {
    if (Data.AssetName.ToString().Equals(AssetName, ESearchCase::IgnoreCase)) {
      Class = Cast<USoundClass>(Data.GetAsset());
      if (Class)
        return Class;
    }
  }
  return nullptr;
}

/**
 * @brief Handle audio-related automation actions described by a JSON payload and perform corresponding editor-side audio operations.
 *
 * Processes actions whose names start with audio_/create_sound_/play_sound_/set_sound_/push_sound_/pop_sound_/create_audio_/create_ambient_/create_reverb_/enable_audio_/fade_sound/set_doppler_/set_audio_/clear_sound_/set_base_sound_/prime_/spawn_sound_. In editor builds this may create audio assets (SoundCue, SoundClass, SoundMix), play or spawn sounds (2D/3D, attached or at location), manage SoundMix state and overrides, fade audio, prime sounds, and create audio components; non-editor builds return a NOT_IMPLEMENTED response.
 *
 * @param RequestId Identifier for the automation request.
 * @param Action Action name to handle (comparison is case-insensitive and matched by known prefixes).
 * @param Payload JSON object containing action parameters (e.g., asset paths, location/rotation arrays, volume, pitch, names).
 * @param RequestingSocket Optional socket that initiated the request (used for sending responses/errors).
 * @return bool `true` if the request was processed (either handled successfully or an error/response was sent); `false` if the action name is not an audio-related command and was not handled. 
 */
bool UMcpAutomationBridgeSubsystem::HandleAudioAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.StartsWith(TEXT("audio_")) &&
      !Lower.StartsWith(TEXT("create_sound_")) &&
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
      !Lower.StartsWith(TEXT("set_audio_")) &&
      !Lower.StartsWith(TEXT("clear_sound_")) &&
      !Lower.StartsWith(TEXT("set_base_sound_")) &&
      !Lower.StartsWith(TEXT("prime_")) &&
      !Lower.StartsWith(TEXT("spawn_sound_"))) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Audio payload missing"), TEXT("INVALID_PAYLOAD"));
    return true;
  }

  if (Lower == TEXT("create_sound_cue") ||
      Lower == TEXT("audio_create_sound_cue")) {
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


    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("path"), SoundCue->GetPathName());
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("SoundCue created"), Resp);
    return true;
  } else if (Lower == TEXT("play_sound_at_location") ||
             Lower == TEXT("audio_play_sound_at_location")) {
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

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("soundPath"), SoundPath);
    TSharedPtr<FJsonObject> LocObj = MakeShared<FJsonObject>();
    LocObj->SetNumberField(TEXT("x"), Location.X);
    LocObj->SetNumberField(TEXT("y"), Location.Y);
    LocObj->SetNumberField(TEXT("z"), Location.Z);
    Resp->SetObjectField(TEXT("location"), LocObj);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Sound played at location"), Resp);
    return true;
  } else if (Lower == TEXT("play_sound_2d") ||
             Lower == TEXT("audio_play_sound_2d")) {
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

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("soundPath"), SoundPath);
    Resp->SetNumberField(TEXT("volume"), Volume);
    Resp->SetNumberField(TEXT("pitch"), Pitch);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Sound played 2D"), Resp);
    return true;
  } else if (Lower == TEXT("create_sound_class") ||
             Lower == TEXT("audio_create_sound_class")) {
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

      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("path"), SoundClass->GetPathName());
      Resp->SetStringField(TEXT("name"), SoundClass->GetName());

      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("SoundClass created"), Resp);
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create SoundClass"),
                          TEXT("ASSET_CREATION_FAILED"));
    }
    return true;
  } else if (Lower == TEXT("create_sound_mix") ||
             Lower == TEXT("audio_create_sound_mix")) {
    FString Name;
    if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("name required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString PackagePath = TEXT("/Game/Audio/Mixes");
    if (Payload->HasField(TEXT("packagePath"))) {
      PackagePath = GetJsonStringField(Payload, TEXT("packagePath"));
    } else if (Payload->HasField(TEXT("savePath"))) {
      PackagePath = GetJsonStringField(Payload, TEXT("savePath"));
    }

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

      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("path"), SoundMix->GetPathName());
      Resp->SetStringField(TEXT("name"), SoundMix->GetName());

      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("SoundMix created"), Resp);
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create SoundMix"),
                          TEXT("ASSET_CREATION_FAILED"));
    }
    return true;
  } else if (Lower == TEXT("push_sound_mix") ||
             Lower == TEXT("audio_push_sound_mix")) {
    FString MixName;
    if (!Payload->TryGetStringField(TEXT("mixName"), MixName) ||
        MixName.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("mixName required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    USoundMix *Mix = ResolveSoundMix(MixName);
    if (Mix) {
      if (GEditor && GEditor->GetEditorWorldContext().World()) {
        UGameplayStatics::PushSoundMixModifier(
            GEditor->GetEditorWorldContext().World(), Mix);
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("mixName"), MixName);
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("SoundMix pushed"), Resp);
      } else {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("No World Context"), TEXT("NO_WORLD"));
      }
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("SoundMix not found"), TEXT("ASSET_NOT_FOUND"));
    }
    return true;
  } else if (Lower == TEXT("pop_sound_mix") ||
             Lower == TEXT("audio_pop_sound_mix")) {
    FString MixName;
    if (!Payload->TryGetStringField(TEXT("mixName"), MixName) ||
        MixName.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("mixName required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    USoundMix *Mix = ResolveSoundMix(MixName);
    if (Mix) {
      if (GEditor && GEditor->GetEditorWorldContext().World()) {
        UGameplayStatics::PopSoundMixModifier(
            GEditor->GetEditorWorldContext().World(), Mix);
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("mixName"), MixName);
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("SoundMix popped"), Resp);
      } else {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("No World Context"), TEXT("NO_WORLD"));
      }
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("SoundMix not found"), TEXT("ASSET_NOT_FOUND"));
    }
    return true;
  } else if (Lower == TEXT("set_sound_mix_class_override") ||
             Lower == TEXT("audio_set_sound_mix_class_override")) {
    FString MixName, ClassName;
    Payload->TryGetStringField(TEXT("mixName"), MixName);
    Payload->TryGetStringField(TEXT("soundClassName"), ClassName);

    USoundMix *Mix = ResolveSoundMix(MixName);
    USoundClass *Class = ResolveSoundClass(ClassName);

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
      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("mixName"), MixName);
      Resp->SetStringField(TEXT("className"), ClassName);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Sound mix override set"), Resp);
    } else {
      SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                          TEXT("NO_WORLD"));
    }
    return true;
  } else if (Lower == TEXT("play_sound_attached") ||
             Lower == TEXT("audio_play_sound_attached")) {
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
             Lower == TEXT("fade_sound_in") ||
             Lower == TEXT("audio_fade_sound_out") ||
             Lower == TEXT("audio_fade_sound_in")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    double FadeTime = 1.0;
    Payload->TryGetNumberField(TEXT("fadeTime"), FadeTime);
    double TargetVol =
        (Lower == TEXT("fade_sound_in") || Lower == TEXT("audio_fade_sound_in"))
            ? 1.0
            : 0.0;
    if (Lower == TEXT("fade_sound_in") || Lower == TEXT("audio_fade_sound_in"))
      Payload->TryGetNumberField(TEXT("targetVolume"), TargetVol);

    if (!GEditor)
      return true;
    UWorld *World = GEditor->GetEditorWorldContext().World();
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
        if (Lower == TEXT("fade_sound_in") ||
            Lower == TEXT("audio_fade_sound_in"))
          AudioComp->FadeIn((float)FadeTime, (float)TargetVol);
        else
          AudioComp->FadeOut((float)FadeTime, (float)TargetVol);

        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("actorName"), ActorName);
        Resp->SetStringField(TEXT("action"), Lower);
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Sound fading"), Resp);
        return true;
      }
    }
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Audio component not found on actor"),
                        TEXT("COMPONENT_NOT_FOUND"));
    return true;
  } else if (Lower == TEXT("create_ambient_sound") ||
             Lower == TEXT("audio_create_ambient_sound")) {
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
  } else if (Lower == TEXT("spawn_sound_at_location") ||
             Lower == TEXT("audio_spawn_sound_at_location")) {
    // Similar to create_ambient_sound but explicit action name
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

    FRotator Rotation = FRotator::ZeroRotator;
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

    if (!GEditor)
      return true;
    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (!World) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                          TEXT("NO_WORLD"));
      return true;
    }

    UAudioComponent *AudioComp = UGameplayStatics::SpawnSoundAtLocation(
        World, Sound, Location, Rotation, (float)Volume, (float)Pitch,
        (float)StartTime, nullptr, nullptr, true);

    if (AudioComp) {
      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetStringField(TEXT("componentName"), AudioComp->GetName());
      Resp->SetStringField(TEXT("componentPath"), AudioComp->GetPathName());
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Sound spawned"), Resp);
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to spawn sound"), TEXT("SPAWN_FAILED"));
    }
    return true;
  } else if (Lower == TEXT("clear_sound_mix_class_override") ||
             Lower == TEXT("audio_clear_sound_mix_class_override")) {
    FString MixName, ClassName;
    Payload->TryGetStringField(TEXT("mixName"), MixName);
    Payload->TryGetStringField(TEXT("soundClassName"), ClassName);

    USoundMix *Mix = ResolveSoundMix(MixName);
    USoundClass *Class = ResolveSoundClass(ClassName);

    if (!Mix || !Class) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Mix or Class not found"),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    double FadeTime = 1.0;
    Payload->TryGetNumberField(TEXT("fadeOutTime"), FadeTime);

    if (GEditor && GEditor->GetEditorWorldContext().World()) {
      UGameplayStatics::ClearSoundMixClassOverride(
          GEditor->GetEditorWorldContext().World(), Mix, Class,
          (float)FadeTime);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Sound mix override cleared"), nullptr);
    } else {
      SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                          TEXT("NO_WORLD"));
    }
    return true;
  } else if (Lower == TEXT("set_base_sound_mix")) {
    FString MixName;
    Payload->TryGetStringField(TEXT("mixName"), MixName);
    USoundMix *Mix = ResolveSoundMix(MixName);
    if (!Mix) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Mix not found"),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    if (GEditor && GEditor->GetEditorWorldContext().World()) {
      UGameplayStatics::SetBaseSoundMix(
          GEditor->GetEditorWorldContext().World(), Mix);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Base sound mix set"), nullptr);
    } else {
      SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                          TEXT("NO_WORLD"));
    }
    return true;
  } else if (Lower == TEXT("prime_sound")) {
    FString SoundPath;
    Payload->TryGetStringField(TEXT("soundPath"), SoundPath);
    USoundBase *Sound = ResolveSoundAsset(SoundPath);
    if (!Sound) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Sound not found"),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    UGameplayStatics::PrimeSound(Sound);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Sound primed"), nullptr);
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
    if (AttachTo.IsEmpty())
      Payload->TryGetStringField(TEXT("actorName"), AttachTo);

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

  if (Lower == TEXT("create_dialogue_voice")) {
    return HandleCreateDialogueVoice(RequestId, Payload, RequestingSocket);
  }
  if (Lower == TEXT("create_dialogue_wave")) {
    return HandleCreateDialogueWave(RequestId, Payload, RequestingSocket);
  }
  if (Lower == TEXT("set_dialogue_context")) {
    return HandleSetDialogueContext(RequestId, Payload, RequestingSocket);
  }
  if (Lower == TEXT("create_reverb_effect")) {
    return HandleCreateReverbEffect(RequestId, Payload, RequestingSocket);
  }
  if (Lower == TEXT("create_source_effect_chain")) {
    return HandleCreateSourceEffectChain(RequestId, Payload, RequestingSocket);
  }
  if (Lower == TEXT("add_source_effect")) {
    return HandleAddSourceEffect(RequestId, Payload, RequestingSocket);
  }
  if (Lower == TEXT("create_submix_effect")) {
    return HandleCreateSubmixEffect(RequestId, Payload, RequestingSocket);
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

// Dialogue Voice handler implementation
bool UMcpAutomationBridgeSubsystem::HandleCreateDialogueVoice(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  FString VoiceName;
  if (!Payload->TryGetStringField(TEXT("voiceName"), VoiceName) ||
      VoiceName.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("voiceName required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString OutputPath;
  if (!Payload->TryGetStringField(TEXT("outputPath"), OutputPath) ||
      OutputPath.IsEmpty()) {
    OutputPath = TEXT("/Game/Audio/Dialogues");
  }

  // Parse gender setting
  FString GenderStr;
  TEnumAsByte<EGrammaticalGender::Type> Gender = EGrammaticalGender::Masculine;
  if (Payload->TryGetStringField(TEXT("gender"), GenderStr)) {
    Gender = GenderStr.Equals(TEXT("Female"), ESearchCase::IgnoreCase)
                 ? EGrammaticalGender::Feminine
                 : EGrammaticalGender::Masculine;
  }

  // Parse pluralization setting
  FString PluralStr;
  TEnumAsByte<EGrammaticalNumber::Type> Plurality = EGrammaticalNumber::Singular;
  if (Payload->TryGetStringField(TEXT("pluralization"), PluralStr)) {
    Plurality = PluralStr.Equals(TEXT("Plural"), ESearchCase::IgnoreCase)
                    ? EGrammaticalNumber::Plural
                    : EGrammaticalNumber::Singular;
  }

  FString FullPath = FString::Printf(TEXT("%s/%s"), *OutputPath, *VoiceName);
  FString PackageName = FullPath;
  if (!PackageName.StartsWith(TEXT("/Game/"))) {
    PackageName = TEXT("/Game/") + PackageName;
  }

  UPackage *Package = CreatePackage(*PackageName);
  if (!Package) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create package"), TEXT("CREATE_FAILED"));
    return true;
  }

  UDialogueVoice *NewVoice = NewObject<UDialogueVoice>(Package, FName(*VoiceName), RF_Public | RF_Standalone);
  if (!NewVoice) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create dialogue voice"), TEXT("CREATE_FAILED"));
    return true;
  }

  // UE 5.7: VoiceName removed, Gender uses EGrammaticalGender, bIsPlural replaced with Plurality
  NewVoice->Gender = Gender;
  NewVoice->Plurality = Plurality;

  Package->MarkPackageDirty();
  FAssetRegistryModule::AssetCreated(NewVoice);

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetStringField(TEXT("voicePath"), NewVoice->GetPathName());
  Resp->SetStringField(TEXT("voiceName"), VoiceName);
  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Dialogue voice created"), Resp);
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

// Dialogue Wave handler implementation
bool UMcpAutomationBridgeSubsystem::HandleCreateDialogueWave(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  FString WaveName;
  if (!Payload->TryGetStringField(TEXT("waveName"), WaveName) ||
      WaveName.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("waveName required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

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
                        TEXT("Sound asset not found"), TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  FString OutputPath;
  if (!Payload->TryGetStringField(TEXT("outputPath"), OutputPath) ||
      OutputPath.IsEmpty()) {
    OutputPath = TEXT("/Game/Audio/Dialogues");
  }

  FString FullPath = FString::Printf(TEXT("%s/%s"), *OutputPath, *WaveName);
  if (!FullPath.StartsWith(TEXT("/Game/"))) {
    FullPath = TEXT("/Game/") + FullPath;
  }

  UPackage *Package = CreatePackage(*FullPath);
  if (!Package) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create package"), TEXT("CREATE_FAILED"));
    return true;
  }

  UDialogueWave *DialogueWave = NewObject<UDialogueWave>(Package, FName(*WaveName), RF_Public | RF_Standalone);
  if (!DialogueWave) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create dialogue wave"), TEXT("CREATE_FAILED"));
    return true;
  }

  // UE 5.7: DialogueVoice renamed to Speaker, SoundWave needs explicit cast from USoundBase
  FDialogueContextMapping Context;
  Context.Context.Speaker = nullptr;
  Context.SoundWave = Cast<USoundWave>(Sound);
  DialogueWave->ContextMappings.Add(Context);

  Package->MarkPackageDirty();
  FAssetRegistryModule::AssetCreated(DialogueWave);

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetStringField(TEXT("wavePath"), DialogueWave->GetPathName());
  Resp->SetStringField(TEXT("waveName"), WaveName);
  Resp->SetStringField(TEXT("soundPath"), SoundPath);
  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Dialogue wave created"), Resp);
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

// Set Dialogue Context handler implementation
bool UMcpAutomationBridgeSubsystem::HandleSetDialogueContext(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  FString WavePath;
  if (!Payload->TryGetStringField(TEXT("wavePath"), WavePath) ||
      WavePath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("wavePath required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  UDialogueWave *DialogueWave = LoadObject<UDialogueWave>(nullptr, *WavePath);
  if (!DialogueWave) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Dialogue wave not found"), TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  FString VoicePath;
  if (!Payload->TryGetStringField(TEXT("voicePath"), VoicePath) ||
      VoicePath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("voicePath required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  UDialogueVoice *Voice = LoadObject<UDialogueVoice>(nullptr, *VoicePath);
  if (!Voice) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Dialogue voice not found"), TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  int32 ContextIndex = 0;
  Payload->TryGetNumberField(TEXT("contextIndex"), ContextIndex);

  if (ContextIndex < 0 || ContextIndex >= DialogueWave->ContextMappings.Num()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Invalid context index"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // UE 5.7: DialogueVoice renamed to Speaker
  DialogueWave->ContextMappings[ContextIndex].Context.Speaker = Voice;
  DialogueWave->MarkPackageDirty();

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetStringField(TEXT("wavePath"), DialogueWave->GetPathName());
  Resp->SetStringField(TEXT("voicePath"), VoicePath);
  Resp->SetNumberField(TEXT("contextIndex"), ContextIndex);
  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Dialogue context set"), Resp);
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

// Create Reverb Effect handler implementation
bool UMcpAutomationBridgeSubsystem::HandleCreateReverbEffect(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  FString EffectName;
  if (!Payload->TryGetStringField(TEXT("effectName"), EffectName) ||
      EffectName.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("effectName required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString OutputPath;
  if (!Payload->TryGetStringField(TEXT("outputPath"), OutputPath) ||
      OutputPath.IsEmpty()) {
    OutputPath = TEXT("/Game/Audio/Effects");
  }

  float Density = 1.0f;
  Payload->TryGetNumberField(TEXT("density"), Density);
  float Diffusion = 1.0f;
  Payload->TryGetNumberField(TEXT("diffusion"), Diffusion);
  float Gain = 0.32f;
  Payload->TryGetNumberField(TEXT("gain"), Gain);
  float GainHF = 0.89f;
  Payload->TryGetNumberField(TEXT("gainHF"), GainHF);
  float DecayTime = 1.49f;
  Payload->TryGetNumberField(TEXT("decayTime"), DecayTime);
  float DecayHFRatio = 0.83f;
  Payload->TryGetNumberField(TEXT("decayHFRatio"), DecayHFRatio);
  float ReflectionsGain = 0.05f;
  Payload->TryGetNumberField(TEXT("reflectionsGain"), ReflectionsGain);
  float LateGain = 1.26f;
  Payload->TryGetNumberField(TEXT("lateGain"), LateGain);

  FString FullPath = FString::Printf(TEXT("%s/%s"), *OutputPath, *EffectName);
  if (!FullPath.StartsWith(TEXT("/Game/"))) {
    FullPath = TEXT("/Game/") + FullPath;
  }

  UPackage *Package = CreatePackage(*FullPath);
  if (!Package) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create package"), TEXT("CREATE_FAILED"));
    return true;
  }

  UReverbEffect *ReverbEffect = NewObject<UReverbEffect>(Package, FName(*EffectName), RF_Public | RF_Standalone);
  if (!ReverbEffect) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create reverb effect"), TEXT("CREATE_FAILED"));
    return true;
  }

  ReverbEffect->Density = Density;
  ReverbEffect->Diffusion = Diffusion;
  ReverbEffect->Gain = Gain;
  ReverbEffect->GainHF = GainHF;
  ReverbEffect->DecayTime = DecayTime;
  ReverbEffect->DecayHFRatio = DecayHFRatio;
  ReverbEffect->ReflectionsGain = ReflectionsGain;
  ReverbEffect->LateGain = LateGain;

  Package->MarkPackageDirty();
  FAssetRegistryModule::AssetCreated(ReverbEffect);

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetStringField(TEXT("effectPath"), ReverbEffect->GetPathName());
  Resp->SetStringField(TEXT("effectName"), EffectName);
  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Reverb effect created"), Resp);
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

// Create Source Effect Chain handler implementation
bool UMcpAutomationBridgeSubsystem::HandleCreateSourceEffectChain(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  FString ChainName;
  if (!Payload->TryGetStringField(TEXT("chainName"), ChainName) ||
      ChainName.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("chainName required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString OutputPath;
  if (!Payload->TryGetStringField(TEXT("outputPath"), OutputPath) ||
      OutputPath.IsEmpty()) {
    OutputPath = TEXT("/Game/Audio/Effects");
  }

  FString FullPath = FString::Printf(TEXT("%s/%s"), *OutputPath, *ChainName);
  if (!FullPath.StartsWith(TEXT("/Game/"))) {
    FullPath = TEXT("/Game/") + FullPath;
  }

  UPackage *Package = CreatePackage(*FullPath);
  if (!Package) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create package"), TEXT("CREATE_FAILED"));
    return true;
  }

  USoundEffectSourcePresetChain *Chain = NewObject<USoundEffectSourcePresetChain>(Package, FName(*ChainName), RF_Public | RF_Standalone);
  if (!Chain) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create source effect chain"), TEXT("CREATE_FAILED"));
    return true;
  }

  Package->MarkPackageDirty();
  FAssetRegistryModule::AssetCreated(Chain);

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetStringField(TEXT("chainPath"), Chain->GetPathName());
  Resp->SetStringField(TEXT("chainName"), ChainName);
  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Source effect chain created"), Resp);
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

// Add Source Effect handler implementation
bool UMcpAutomationBridgeSubsystem::HandleAddSourceEffect(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  FString ChainPath;
  if (!Payload->TryGetStringField(TEXT("chainPath"), ChainPath) ||
      ChainPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("chainPath required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  USoundEffectSourcePresetChain *Chain = LoadObject<USoundEffectSourcePresetChain>(nullptr, *ChainPath);
  if (!Chain) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Source effect chain not found"), TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  FString EffectType;
  if (!Payload->TryGetStringField(TEXT("effectType"), EffectType) ||
      EffectType.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("effectType required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString EffectName;
  Payload->TryGetStringField(TEXT("effectName"), EffectName);
  if (EffectName.IsEmpty()) {
    EffectName = FString::Printf(TEXT("Effect_%d"), Chain->Chain.Num());
  }

  FSourceEffectChainEntry Entry;
  Entry.bBypass = false;

  if (EffectType.Equals(TEXT("EQ"), ESearchCase::IgnoreCase)) {
    USoundEffectSourcePreset *EQPreset = NewObject<USoundEffectSourcePreset>();
    Entry.Preset = EQPreset;
  } else if (EffectType.Equals(TEXT("Reverb"), ESearchCase::IgnoreCase)) {
    USoundEffectSourcePreset *ReverbPreset = NewObject<USoundEffectSourcePreset>();
    Entry.Preset = ReverbPreset;
  } else if (EffectType.Equals(TEXT("Delay"), ESearchCase::IgnoreCase)) {
    USoundEffectSourcePreset *DelayPreset = NewObject<USoundEffectSourcePreset>();
    Entry.Preset = DelayPreset;
  } else {
    SendAutomationError(RequestingSocket, RequestId,
                        FString::Printf(TEXT("Unknown effect type: %s"), *EffectType),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  Chain->Chain.Add(Entry);
  Chain->MarkPackageDirty();

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetStringField(TEXT("chainPath"), Chain->GetPathName());
  Resp->SetStringField(TEXT("effectType"), EffectType);
  Resp->SetStringField(TEXT("effectName"), EffectName);
  Resp->SetNumberField(TEXT("effectIndex"), Chain->Chain.Num() - 1);
  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Source effect added to chain"), Resp);
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

// Create Submix Effect handler implementation
bool UMcpAutomationBridgeSubsystem::HandleCreateSubmixEffect(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  FString EffectName;
  if (!Payload->TryGetStringField(TEXT("effectName"), EffectName) ||
      EffectName.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("effectName required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString OutputPath;
  if (!Payload->TryGetStringField(TEXT("outputPath"), OutputPath) ||
      OutputPath.IsEmpty()) {
    OutputPath = TEXT("/Game/Audio/Effects");
  }

  FString EffectType;
  if (!Payload->TryGetStringField(TEXT("effectType"), EffectType) ||
      EffectType.IsEmpty()) {
    EffectType = TEXT("Reverb");
  }

  FString FullPath = FString::Printf(TEXT("%s/%s"), *OutputPath, *EffectName);
  if (!FullPath.StartsWith(TEXT("/Game/"))) {
    FullPath = TEXT("/Game/") + FullPath;
  }

  UPackage *Package = CreatePackage(*FullPath);
  if (!Package) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create package"), TEXT("CREATE_FAILED"));
    return true;
  }

  USoundEffectSubmixPreset *SubmixEffect = NewObject<USoundEffectSubmixPreset>(Package, FName(*EffectName), RF_Public | RF_Standalone);
  if (!SubmixEffect) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create submix effect"), TEXT("CREATE_FAILED"));
    return true;
  }

  Package->MarkPackageDirty();
  FAssetRegistryModule::AssetCreated(SubmixEffect);

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetStringField(TEXT("effectPath"), SubmixEffect->GetPathName());
  Resp->SetStringField(TEXT("effectName"), EffectName);
  Resp->SetStringField(TEXT("effectType"), EffectType);
  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Submix effect created"), Resp);
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}