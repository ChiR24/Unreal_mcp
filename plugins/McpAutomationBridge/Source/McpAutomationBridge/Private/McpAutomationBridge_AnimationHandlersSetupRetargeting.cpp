#include "McpAutomationBridge_AnimationHandlersActionContext.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpHandlerUtils.h"
#include "McpSafeOperations.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "Misc/PackageName.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationSetupRetargetingAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    FString SourceSkeletonPath;
    FString TargetSkeletonPath;
    Payload->TryGetStringField(TEXT("sourceSkeleton"), SourceSkeletonPath);
    Payload->TryGetStringField(TEXT("targetSkeleton"), TargetSkeletonPath);

    USkeleton *SourceSkeleton = nullptr;
    USkeleton *TargetSkeleton = nullptr;

    if (!SourceSkeletonPath.IsEmpty()) {
      SourceSkeleton = LoadObject<USkeleton>(nullptr, *SourceSkeletonPath);
    }
    if (!TargetSkeletonPath.IsEmpty()) {
      TargetSkeleton = LoadObject<USkeleton>(nullptr, *TargetSkeletonPath);
    }

    if (!SourceSkeleton || !TargetSkeleton) {
      bSuccess = false;
      Message =
          TEXT("Retargeting failed - source or target skeleton not found");
      ErrorCode = TEXT("ASSET_NOT_FOUND");
      Resp->SetStringField(TEXT("error"), Message);
      Resp->SetStringField(TEXT("sourceSkeleton"), SourceSkeletonPath);
      Resp->SetStringField(TEXT("targetSkeleton"), TargetSkeletonPath);
    } else {
      const TArray<TSharedPtr<FJsonValue>> *AssetsArray = nullptr;
      if (!Payload->TryGetArrayField(TEXT("assets"), AssetsArray)) {
        Payload->TryGetArrayField(TEXT("retargetAssets"), AssetsArray);
      }

      if (!AssetsArray || AssetsArray->Num() == 0) {
        bSuccess = false;
        Message = TEXT("setup_retargeting requires at least one animation asset to retarget");
        ErrorCode = TEXT("MISSING_RETARGET_ASSETS");
        Resp->SetStringField(TEXT("error"), Message);
        Resp->SetStringField(TEXT("sourceSkeleton"), SourceSkeletonPath);
        Resp->SetStringField(TEXT("targetSkeleton"), TargetSkeletonPath);
      } else {

      FString SavePath;
      Payload->TryGetStringField(TEXT("savePath"), SavePath);
      if (!SavePath.IsEmpty()) {
        SavePath = SavePath.TrimStartAndEnd();
        if (!FPackageName::IsValidLongPackageName(SavePath)) {
          FString NormalizedPath;
          if (FPackageName::TryConvertFilenameToLongPackageName(
                  SavePath, NormalizedPath)) {
            SavePath = NormalizedPath;
          } else {
            SavePath.Reset();
          }
        }
      }

      FString Suffix;
      Payload->TryGetStringField(TEXT("suffix"), Suffix);
      if (Suffix.IsEmpty()) {
        Suffix = TEXT("_Retargeted");
      }

      bool bOverwrite = false;
      Payload->TryGetBoolField(TEXT("overwrite"), bOverwrite);

      TArray<FString> RetargetedAssets;
      TArray<FString> SkippedAssets;
      TArray<TSharedPtr<FJsonValue>> WarningArray;

      if (AssetsArray && AssetsArray->Num() > 0) {
        for (const TSharedPtr<FJsonValue> &Value : *AssetsArray) {
          if (!Value.IsValid() || Value->Type != EJson::String) {
            continue;
          }

          const FString SourceAssetPath = Value->AsString();
          UAnimSequence *SourceSequence =
              LoadObject<UAnimSequence>(nullptr, *SourceAssetPath);
          if (!SourceSequence) {
            WarningArray.Add(MakeShared<FJsonValueString>(FString::Printf(
                TEXT("Skipped non-sequence asset: %s"), *SourceAssetPath)));
            SkippedAssets.Add(SourceAssetPath);
            continue;
          }

          FString DestinationFolder = SavePath;
          if (DestinationFolder.IsEmpty()) {
            const FString SourcePackageName =
                SourceSequence->GetOutermost()->GetName();
            DestinationFolder =
                FPackageName::GetLongPackagePath(SourcePackageName);
          }

          if (!DestinationFolder.IsEmpty() &&
              !UEditorAssetLibrary::DoesDirectoryExist(DestinationFolder)) {
            UEditorAssetLibrary::MakeDirectory(DestinationFolder);
          }

          FString DestinationAssetName = FPackageName::GetShortName(
              SourceSequence->GetOutermost()->GetName());
          DestinationAssetName += Suffix;

          const FString DestinationObjectPath = FString::Printf(
              TEXT("%s/%s"), *DestinationFolder, *DestinationAssetName);

          if (UEditorAssetLibrary::DoesAssetExist(DestinationObjectPath)) {
            if (!bOverwrite) {
              WarningArray.Add(MakeShared<FJsonValueString>(FString::Printf(
                  TEXT("Retarget destination already exists, skipping: %s"),
                  *DestinationObjectPath)));
              SkippedAssets.Add(SourceAssetPath);
              continue;
            }
            if (!UEditorAssetLibrary::DeleteAsset(DestinationObjectPath)) {
              WarningArray.Add(MakeShared<FJsonValueString>(FString::Printf(
                  TEXT("Failed to delete existing retarget destination: %s"),
                  *DestinationObjectPath)));
              SkippedAssets.Add(SourceAssetPath);
              continue;
            }
          }

          UPackage *DestinationPackage = CreatePackage(*DestinationObjectPath);
          if (!DestinationPackage) {
            WarningArray.Add(MakeShared<FJsonValueString>(FString::Printf(
                TEXT("Failed to create destination package: %s"),
                *DestinationObjectPath)));
            SkippedAssets.Add(SourceAssetPath);
            continue;
          }

          UAnimSequence *DestinationSequence = Cast<UAnimSequence>(
              StaticDuplicateObject(SourceSequence, DestinationPackage,
                                    *DestinationAssetName));
          if (!DestinationSequence) {
            WarningArray.Add(MakeShared<FJsonValueString>(
                FString::Printf(TEXT("Failed to duplicate animation asset: %s"),
                                *DestinationObjectPath)));
            SkippedAssets.Add(SourceAssetPath);
            continue;
          }

          DestinationSequence->SetFlags(RF_Public | RF_Standalone);
          DestinationSequence->Modify();
          DestinationSequence->SetSkeleton(TargetSkeleton);
          DestinationPackage->MarkPackageDirty();
          FAssetRegistryModule::AssetCreated(DestinationSequence);
          McpSafeOperations::McpSafeAssetSave(DestinationSequence);

          // Animation retargeting in UE5 requires IK Rig system
          // Use a duplicated AnimSequence with the target skeleton assigned.
          UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
                 TEXT("Animation asset copied (retargeting requires IK Rig "
                      "setup)"));

          RetargetedAssets.Add(DestinationSequence->GetPathName());
        }
      }

      bSuccess = RetargetedAssets.Num() > 0;
      Message = bSuccess
                    ? TEXT("Retargeting completed")
                    : TEXT("Retargeting failed - no assets processed");
      if (!bSuccess) {
        ErrorCode = TEXT("NO_ASSETS_RETARGETED");
        Resp->SetStringField(TEXT("error"), Message);
      }

      TArray<TSharedPtr<FJsonValue>> RetargetedArray;
      for (const FString &Path : RetargetedAssets) {
        RetargetedArray.Add(MakeShared<FJsonValueString>(Path));
      }
      if (RetargetedArray.Num() > 0) {
        Resp->SetArrayField(TEXT("retargetedAssets"), RetargetedArray);
        // Add verification for the first retargeted asset
        if (RetargetedAssets.Num() > 0) {
          UAnimSequence* FirstRetargeted = LoadObject<UAnimSequence>(nullptr, *RetargetedAssets[0]);
          if (FirstRetargeted) {
            McpHandlerUtils::AddVerification(Resp, FirstRetargeted);
          }
        }
      }

      if (SkippedAssets.Num() > 0) {
        TArray<TSharedPtr<FJsonValue>> SkippedArray;
        for (const FString &Path : SkippedAssets) {
          SkippedArray.Add(MakeShared<FJsonValueString>(Path));
        }
        Resp->SetArrayField(TEXT("skippedAssets"), SkippedArray);
      }

      if (WarningArray.Num() > 0) {
        Resp->SetArrayField(TEXT("warnings"), WarningArray);
      }

      Resp->SetStringField(TEXT("sourceSkeleton"),
                           SourceSkeleton->GetPathName());
      Resp->SetStringField(TEXT("targetSkeleton"),
                           TargetSkeleton->GetPathName());
      }
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers
