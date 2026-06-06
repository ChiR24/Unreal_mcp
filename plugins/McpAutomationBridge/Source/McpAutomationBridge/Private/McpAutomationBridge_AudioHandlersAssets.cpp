#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AudioHandlersPrivate.h"

namespace McpAudioHandlers
{
#if WITH_EDITOR
bool HandleAssetActions(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const FString& Lower,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (Lower == TEXT("create_sound_cue") ||
      Lower == TEXT("audio_create_sound_cue")) {
    FString Name;
    if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
      Self->SendAutomationError(RequestingSocket, RequestId, TEXT("name required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString PackagePath;
    Payload->TryGetStringField(TEXT("packagePath"), PackagePath);
    if (PackagePath.IsEmpty())
      PackagePath = TEXT("/Game/Audio/Cues");

    FString FullPath;
    if (!BuildSanitizedAssetPath(PackagePath, Name, PackagePath, FullPath)) {
      Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Invalid path"),
                          TEXT("INVALID_PATH"));
      return true;
    }

    const FString FullObjectPath = FString::Printf(TEXT("%s.%s"), *FullPath, *Name);
    USoundCue *ExistingSoundCue = nullptr;
    if (UEditorAssetLibrary::DoesAssetExist(FullPath)) {
      ExistingSoundCue = Cast<USoundCue>(UEditorAssetLibrary::LoadAsset(FullPath));
    }
    if (!ExistingSoundCue) {
      ExistingSoundCue = FindObject<USoundCue>(nullptr, *FullObjectPath);
    }
    if (!ExistingSoundCue) {
      ExistingSoundCue = LoadObject<USoundCue>(nullptr, *FullObjectPath);
    }
    if (ExistingSoundCue) {
      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("path"), ExistingSoundCue->GetPathName());
      Resp->SetStringField(TEXT("assetPath"), FullPath);
      Resp->SetStringField(TEXT("assetName"), Name);
      McpHandlerUtils::AddVerification(Resp, ExistingSoundCue);
      Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("SoundCue already exists"), Resp);
      return true;
    }

    FString WavePath;
    Payload->TryGetStringField(TEXT("wavePath"), WavePath);

    USoundWave *Wave = nullptr;
    if (!WavePath.IsEmpty()) {
      Wave = LoadObject<USoundWave>(nullptr, *WavePath);
    }

    USoundCueFactoryNew *Factory = NewObject<USoundCueFactoryNew>();
    if (Wave) {
      Factory->InitialSoundWaves.Add(Wave);
    }
    FAssetToolsModule &AssetToolsModule =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
    UObject *NewAsset = AssetToolsModule.Get().CreateAsset(
        Name, PackagePath, USoundCue::StaticClass(), Factory);
    USoundCue *SoundCue = Cast<USoundCue>(NewAsset);

    if (!SoundCue) {
      Self->SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create SoundCue"),
                          TEXT("ASSET_CREATION_FAILED"));
      return true;
    }

    // Basic graph setup if wave provided
    if (Wave) {
      USoundNode *LastNode = SoundCue->FirstNode;

      if (LastNode) {
        bool bLooping = false;
        if (Payload->TryGetBoolField(TEXT("looping"), bLooping) && bLooping) {
          USoundNodeLooping *LoopNode =
              SoundCue->ConstructSoundNode<USoundNodeLooping>();
          LoopNode->InsertChildNode(0);
          LoopNode->ChildNodes[0] = LastNode;
          LastNode = LoopNode;
        }

        double Volume = 1.0;
        double Pitch = 1.0;
        const bool bHasVolume = Payload->TryGetNumberField(TEXT("volume"), Volume);
        const bool bHasPitch = Payload->TryGetNumberField(TEXT("pitch"), Pitch);

        if (bHasVolume || bHasPitch) {
          USoundNodeModulator *ModNode =
              SoundCue->ConstructSoundNode<USoundNodeModulator>();
          ModNode->InsertChildNode(0);
          ModNode->ChildNodes[0] = LastNode;
          ModNode->PitchMin = ModNode->PitchMax = (float)Pitch;
          ModNode->VolumeMin = ModNode->VolumeMax = (float)Volume;
          LastNode = ModNode;
        }

        FString AttenuationPath;
        if (Payload->TryGetStringField(TEXT("attenuationPath"),
                                       AttenuationPath) &&
            !AttenuationPath.IsEmpty()) {
          USoundAttenuation *Attenuation =
              LoadObject<USoundAttenuation>(nullptr, *AttenuationPath);
          if (Attenuation) {
            USoundNodeAttenuation *AttenNode =
                SoundCue->ConstructSoundNode<USoundNodeAttenuation>();
            AttenNode->InsertChildNode(0);
            AttenNode->ChildNodes[0] = LastNode;
            AttenNode->AttenuationSettings = Attenuation;
            LastNode = AttenNode;
          }
        }

        SoundCue->FirstNode = LastNode;
        SoundCue->LinkGraphNodesFromSoundNodes();
      }
    }


    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("path"), SoundCue->GetPathName());
    McpHandlerUtils::AddVerification(Resp, SoundCue);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("SoundCue created"), Resp);
    return true;
  }

  // -------------------------------------------------------------------------
  // create_sound_class / audio_create_sound_class
  // -------------------------------------------------------------------------
  // Creates a USoundClass asset with optional volume/pitch properties and parent.
  //
  // Payload:  { "name": string, "properties"?: { "volume"?: number, "pitch"?: number },
  //             "parentClass"?: string }
  // Response: { "success": bool, "path": string, "name": string }
  // -------------------------------------------------------------------------
  if (Lower == TEXT("create_sound_class") ||
             Lower == TEXT("audio_create_sound_class")) {
    FString Name;
    if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
      Self->SendAutomationError(RequestingSocket, RequestId, TEXT("name required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString PackagePath = TEXT("/Game/Audio/Classes");
    if (Payload->HasField(TEXT("path"))) {
      PackagePath = GetJsonStringField(Payload, TEXT("path"));
    }

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

      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("path"), SoundClass->GetPathName());
      Resp->SetStringField(TEXT("name"), SoundClass->GetName());

      McpHandlerUtils::AddVerification(Resp, SoundClass);
      Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("SoundClass created"), Resp);
    } else {
      Self->SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create SoundClass"),
                          TEXT("ASSET_CREATION_FAILED"));
    }
    return true;
  }

  // -------------------------------------------------------------------------
  // create_sound_mix / audio_create_sound_mix
  // -------------------------------------------------------------------------
  // Creates a USoundMix asset with optional class adjusters for volume/pitch.
  //
  // Payload:  { "name": string, "packagePath"?: string, "savePath"?: string,
  //             "classAdjusters"?: [{ "soundClass": string,
  //             "volumeAdjuster"?: number, "pitchAdjuster"?: number }] }
  // Response: { "success": bool, "path": string, "name": string }
  // -------------------------------------------------------------------------
  else if (Lower == TEXT("create_sound_mix") ||
             Lower == TEXT("audio_create_sound_mix")) {
    FString Name;
    if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
      Self->SendAutomationError(RequestingSocket, RequestId, TEXT("name required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString PackagePath = TEXT("/Game/Audio/Mixes");
    if (Payload->HasField(TEXT("packagePath"))) {
      PackagePath = GetJsonStringField(Payload, TEXT("packagePath"));
    } else if (Payload->HasField(TEXT("savePath"))) {
      PackagePath = GetJsonStringField(Payload, TEXT("savePath"));
    } else if (Payload->HasField(TEXT("path"))) {
      PackagePath = GetJsonStringField(Payload, TEXT("path"));
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

      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("path"), SoundMix->GetPathName());
      Resp->SetStringField(TEXT("name"), SoundMix->GetName());

      McpHandlerUtils::AddVerification(Resp, SoundMix);
      Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("SoundMix created"), Resp);
    } else {
      Self->SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create SoundMix"),
                          TEXT("ASSET_CREATION_FAILED"));
    }
    return true;
  }
  return false;
}
#endif
}
