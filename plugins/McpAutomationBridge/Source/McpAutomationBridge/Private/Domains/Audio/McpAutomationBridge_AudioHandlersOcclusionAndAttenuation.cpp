#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Audio/McpAutomationBridge_AudioHandlersPrivate.h"

namespace McpAudioHandlers
{
#if WITH_EDITOR
bool HandleSpatialActions(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const FString& Lower,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (Lower == TEXT("set_audio_occlusion")) {
    FString SoundPath;
    Payload->TryGetStringField(TEXT("soundPath"), SoundPath);

    bool bEnable = true;
    Payload->TryGetBoolField(TEXT("enable"), bEnable);

    double OcclusionVolumeScale = 0.5;
    Payload->TryGetNumberField(TEXT("occlusionVolumeScale"), OcclusionVolumeScale);

    double OcclusionFilterScale = 0.5;
    Payload->TryGetNumberField(TEXT("occlusionFilterScale"), OcclusionFilterScale);

    double OcclusionInterpolationTime = 0.1;
    Payload->TryGetNumberField(TEXT("occlusionInterpolationTime"), OcclusionInterpolationTime);

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);

    USoundAttenuation* AttenuationSettings = nullptr;
    USoundBase* Sound = nullptr;
    bool bAttenuationCreated = false;

    if (!SoundPath.IsEmpty()) {
      // Validate path for security
      FString ValidatedPath = McpHandlerUtils::ValidateAssetPath(SoundPath);
      if (ValidatedPath.IsEmpty()) {
        Self->SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Invalid sound path"), TEXT("INVALID_PATH"));
        return true;
      }

      // soundPath names a sound (the contract); a USoundAttenuation asset is still accepted.
      UObject* Loaded = StaticLoadObject(UObject::StaticClass(), nullptr, *ValidatedPath);
      AttenuationSettings = Cast<USoundAttenuation>(Loaded);
      Sound = Cast<USoundBase>(Loaded);
      if (!AttenuationSettings && !Sound) {
        Sound = ResolveSoundAsset(ValidatedPath);
      }
      if (!AttenuationSettings && !Sound) {
        Self->SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Sound or sound attenuation not found: %s"), *SoundPath),
                            TEXT("ASSET_NOT_FOUND"));
        return true;
      }
      if (Sound) {
        AttenuationSettings = Sound->AttenuationSettings;
        if (!AttenuationSettings) {
          // The sound has no attenuation asset yet: author one next to it and assign it,
          // so the occlusion settings actually reach playback (dogfood #111).
          const FString AttenName = Sound->GetName() + TEXT("_Attenuation");
          const FString AttenPackageName =
              FPackageName::GetLongPackagePath(Sound->GetOutermost()->GetName()) + TEXT("/") + AttenName;
          UPackage* AttenPackage = CreatePackage(*AttenPackageName);
          AttenuationSettings = AttenPackage
              ? NewObject<USoundAttenuation>(AttenPackage, FName(*AttenName), RF_Public | RF_Standalone)
              : nullptr;
          if (!AttenuationSettings) {
            Self->SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Failed to create %s"), *AttenPackageName),
                                TEXT("CREATE_FAILED"));
            return true;
          }
          FAssetRegistryModule::AssetCreated(AttenuationSettings);
          Sound->Modify();
          Sound->AttenuationSettings = AttenuationSettings;
          Sound->MarkPackageDirty();
          bAttenuationCreated = true;
        }
      }
    } else {
      // Create a new attenuation settings for occlusion configuration
      AttenuationSettings = NewObject<USoundAttenuation>(GetTransientPackage(),
                                                          FName(TEXT("TempOcclusionSettings")));
    }

    if (AttenuationSettings) {
      // Occlusion settings are in the Attenuation subobject (FSoundAttenuationSettings)
      // Enable/disable occlusion
      AttenuationSettings->Attenuation.bEnableOcclusion = bEnable;

      // Set occlusion parameters
      AttenuationSettings->Attenuation.OcclusionVolumeAttenuation = (float)OcclusionVolumeScale;
      // OcclusionFilterScale maps to OcclusionLowPassFilterFrequency (scaled value)
      // Higher filter scale = higher frequency = less filtering
      AttenuationSettings->Attenuation.OcclusionLowPassFilterFrequency = (float)(20000.0 * OcclusionFilterScale);
      AttenuationSettings->Attenuation.OcclusionInterpolationTime = (float)OcclusionInterpolationTime;

      AttenuationSettings->MarkPackageDirty();
	if (bSave && !SoundPath.IsEmpty()) {
		if (!McpSafeAssetSave(AttenuationSettings)) {
			Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to save attenuation settings"), TEXT("SAVE_FAILED"));
			return true;
		}
		if (bAttenuationCreated && !McpSafeAssetSave(Sound)) {
			Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to save the sound after assigning its attenuation"), TEXT("SAVE_FAILED"));
			return true;
		}
	}

      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetBoolField(TEXT("enabled"), bEnable);
      Resp->SetNumberField(TEXT("occlusionVolumeScale"), OcclusionVolumeScale);
      Resp->SetNumberField(TEXT("occlusionFilterScale"), OcclusionFilterScale);
      Resp->SetNumberField(TEXT("occlusionInterpolationTime"), OcclusionInterpolationTime);
      if (!SoundPath.IsEmpty()) {
        Resp->SetStringField(TEXT("soundPath"), Sound ? Sound->GetPathName() : SoundPath);
        Resp->SetStringField(TEXT("attenuationPath"), AttenuationSettings->GetPathName());
        Resp->SetBoolField(TEXT("attenuationCreated"), bAttenuationCreated);
        Resp->SetBoolField(TEXT("assignedToSound"), Sound != nullptr);
        McpHandlerUtils::AddVerification(Resp, AttenuationSettings);
      }
      Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Audio occlusion configured"), Resp);
    } else {
      Self->SendAutomationError(RequestingSocket, RequestId,
                           TEXT("Failed to configure audio occlusion"), TEXT("CONFIGURATION_FAILED"));
     }
     return true;
   }

   if (Lower == TEXT("set_sound_attenuation") || Lower == TEXT("audio_set_sound_attenuation")) {
     FString Name;
     if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
       Self->SendAutomationError(RequestingSocket, RequestId,
                           TEXT("name required"), TEXT("INVALID_ARGUMENT"));
       return true;
     }

     FString PackagePath = TEXT("/Game/Audio/Attenuation");
     Payload->TryGetStringField(TEXT("path"), PackagePath);

     double InnerRadius = 400.0;
     Payload->TryGetNumberField(TEXT("innerRadius"), InnerRadius);
      double FalloffDistance = 3600.0;
      Payload->TryGetNumberField(TEXT("falloffDistance"), FalloffDistance);
      FString AttenuationShape = TEXT("Sphere");
      Payload->TryGetStringField(TEXT("attenuationShape"), AttenuationShape);
      FString FalloffMode = TEXT("Linear");
      Payload->TryGetStringField(TEXT("falloffMode"), FalloffMode);
     bool bSave = true;
     Payload->TryGetBoolField(TEXT("save"), bSave);

     FString FullPath;
     if (!BuildSanitizedAssetPath(PackagePath, Name, PackagePath, FullPath)) {
       Self->SendAutomationError(RequestingSocket, RequestId,
                           TEXT("Invalid path"), TEXT("INVALID_PATH"));
       return true;
     }

     UPackage *Package = CreatePackage(*FullPath);
     if (!Package) {
       Self->SendAutomationError(RequestingSocket, RequestId,
                           TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
       return true;
     }

     USoundAttenuationFactory* Factory = NewObject<USoundAttenuationFactory>();
     USoundAttenuation *Atten = Factory ? Cast<USoundAttenuation>(
         Factory->FactoryCreateNew(USoundAttenuation::StaticClass(), Package,
                                   FName(*Name), RF_Public | RF_Standalone,
                                   nullptr, GWarn)) : nullptr;
     if (!Atten) {
       Self->SendAutomationError(RequestingSocket, RequestId,
                           TEXT("Failed to create SoundAttenuation"), TEXT("CREATE_FAILED"));
       return true;
     }

       // Configure attenuation settings
       Atten->Attenuation.AttenuationShapeExtents.X = (float)InnerRadius;
       Atten->Attenuation.FalloffDistance = (float)FalloffDistance;

      FString AppliedShape = TEXT("Sphere");
      if (AttenuationShape.Equals(TEXT("Capsule"), ESearchCase::IgnoreCase)) {
        Atten->Attenuation.AttenuationShape = EAttenuationShape::Capsule;
        AppliedShape = TEXT("Capsule");
      } if (AttenuationShape.Equals(TEXT("Box"), ESearchCase::IgnoreCase)) {
        Atten->Attenuation.AttenuationShape = EAttenuationShape::Box;
        AppliedShape = TEXT("Box");
      } else if (AttenuationShape.Equals(TEXT("Cone"), ESearchCase::IgnoreCase)) {
        Atten->Attenuation.AttenuationShape = EAttenuationShape::Cone;
        AppliedShape = TEXT("Cone");
      } else {
        Atten->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
      }

      FString AppliedFalloffMode = TEXT("Linear");
      if (FalloffMode.Equals(TEXT("Logarithmic"), ESearchCase::IgnoreCase)) {
        Atten->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Logarithmic;
        AppliedFalloffMode = TEXT("Logarithmic");
      } else if (FalloffMode.Equals(TEXT("Inverse"), ESearchCase::IgnoreCase)) {
        Atten->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Inverse;
        AppliedFalloffMode = TEXT("Inverse");
      } else if (FalloffMode.Equals(TEXT("NaturalSound"), ESearchCase::IgnoreCase)) {
        Atten->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
        AppliedFalloffMode = TEXT("NaturalSound");
      } else {
        Atten->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Linear;
      }

      FAssetRegistryModule::AssetCreated(Atten);
      Package->MarkPackageDirty();

      if (bSave) {
        if (!McpSafeAssetSave(Atten)) {
         Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to save sound attenuation asset"), TEXT("SAVE_FAILED"));
         return true;
       }
     }

     TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("path"), Atten->GetPathName());
      Resp->SetStringField(TEXT("name"), Name);
      Resp->SetNumberField(TEXT("innerRadius"), InnerRadius);
      Resp->SetNumberField(TEXT("falloffDistance"), FalloffDistance);
      Resp->SetStringField(TEXT("attenuationShape"), AppliedShape);
      Resp->SetStringField(TEXT("falloffMode"), AppliedFalloffMode);
      McpHandlerUtils::AddVerification(Resp, Atten);
     Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                            TEXT("Sound attenuation configured"), Resp);
     return true;
   }
  return false;
}
#endif
}
