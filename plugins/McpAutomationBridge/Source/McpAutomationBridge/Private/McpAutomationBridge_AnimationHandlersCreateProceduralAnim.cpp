#include "McpAutomationBridge_AnimationHandlersActionContext.h"
#include "McpAutomationBridge_AnimationHandlersProceduralTracks.h"
#include "McpHandlerUtils.h"
#include "McpSafeOperations.h"
#include "McpVersionCompatibility.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "EditorAssetLibrary.h"
#include "Factories/AnimSequenceFactory.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationCreateProceduralAnimAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    FString Name;
    if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
      Message = TEXT("name field required for procedural animation creation");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      FString SavePath;
      Payload->TryGetStringField(TEXT("savePath"), SavePath);
      if (SavePath.IsEmpty()) {
        Payload->TryGetStringField(TEXT("path"), SavePath);
      }
      if (SavePath.IsEmpty()) {
        SavePath = TEXT("/Game/Animations");
      }

      FString SkeletonPath;
      if (!Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath) ||
          SkeletonPath.IsEmpty()) {
        Message = TEXT("skeletonPath is required for create_procedural_anim");
        ErrorCode = TEXT("INVALID_ARGUMENT");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        const TArray<TSharedPtr<FJsonValue>> *BoneTracksArray = nullptr;
        if (!Payload->TryGetArrayField(TEXT("boneTracks"), BoneTracksArray) ||
            !BoneTracksArray) {
          Message =
              TEXT("boneTracks array is required for create_procedural_anim");
          ErrorCode = TEXT("INVALID_ARGUMENT");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          double NumFramesNumber = 30.0;
          Payload->TryGetNumberField(TEXT("numFrames"), NumFramesNumber);
          const int32 NumFrames =
              FMath::Max(1, static_cast<int32>(NumFramesNumber));

          double FrameRateNumber = 30.0;
          Payload->TryGetNumberField(TEXT("frameRate"), FrameRateNumber);
          const int32 FrameRate =
              FMath::Max(1, static_cast<int32>(FrameRateNumber));

          bool bShouldSave = true;
          Payload->TryGetBoolField(TEXT("save"), bShouldSave);

          USkeleton *TargetSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
          if (!TargetSkeleton) {
            Message = TEXT("Failed to load skeleton for procedural animation");
            ErrorCode = TEXT("LOAD_FAILED");
            Resp->SetStringField(TEXT("error"), Message);
          } else {
            // Pre-check for existing asset to avoid UObject type-collision
            // crashes. Follow same pattern as create_montage/create_sequence.
            const FString ObjectPath =
                FString::Printf(TEXT("%s/%s"), *SavePath, *Name);
            if (UEditorAssetLibrary::DoesAssetExist(ObjectPath)) {
              UObject *ExistingAsset = UEditorAssetLibrary::LoadAsset(ObjectPath);
              if (ExistingAsset) {
                if (UAnimSequence *ExistingSequence =
                        Cast<UAnimSequence>(ExistingAsset)) {
                  bSuccess = true;
                  Message =
                      FString::Printf(TEXT("Procedural animation '%s' already "
                                           "exists - reusing existing asset"),
                                      *Name);
                  Resp->SetStringField(TEXT("assetPath"), ObjectPath);
                  Resp->SetBoolField(TEXT("existingAsset"), true);
                  Resp->SetStringField(TEXT("skeletonPath"),
                                       ExistingSequence->GetSkeleton()
                                           ? ExistingSequence->GetSkeleton()->GetPathName()
                                           : SkeletonPath);
                  McpHandlerUtils::AddVerification(Resp, ExistingSequence);
                } else {
                  Message = FString::Printf(
                      TEXT("Cannot create procedural animation: asset '%s' "
                           "already exists as type '%s'"),
                      *ObjectPath, *ExistingAsset->GetClass()->GetName());
                  ErrorCode = TEXT("ASSET_TYPE_MISMATCH");
                  Resp->SetStringField(TEXT("error"), Message);
                }
              } else {
                Message =
                    FString::Printf(TEXT("Asset exists but failed to load: %s"),
                                    *ObjectPath);
                ErrorCode = TEXT("ASSET_LOAD_FAILED");
                Resp->SetStringField(TEXT("error"), Message);
              }
            } else {
              FString PackagePath = SavePath / Name;
              UPackage *Package = CreatePackage(*PackagePath);
              if (!Package) {
                Message = TEXT("Failed to create package for animation sequence");
                ErrorCode = TEXT("PACKAGE_ERROR");
                Resp->SetStringField(TEXT("error"), Message);
              } else {
                UAnimSequenceFactory *Factory = NewObject<UAnimSequenceFactory>();
                if (!Factory) {
                  Message = TEXT("Failed to create AnimSequence factory");
                  ErrorCode = TEXT("FACTORY_FAILED");
                  Resp->SetStringField(TEXT("error"), Message);
                } else {
                  Factory->TargetSkeleton = TargetSkeleton;

                  UAnimSequence *NewSequence = Cast<UAnimSequence>(
                      Factory->FactoryCreateNew(UAnimSequence::StaticClass(), Package,
                                                FName(*Name),
                                                RF_Public | RF_Standalone,
                                                nullptr, GWarn));

                  if (!NewSequence) {
                    Message = TEXT("Failed to create procedural animation "
                                   "sequence asset");
                    ErrorCode = TEXT("ASSET_CREATION_FAILED");
                    Resp->SetStringField(TEXT("error"), Message);
                  } else {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
                    NewSequence->GetController().SetFrameRate(
                        FFrameRate(FrameRate, 1));
                    NewSequence->GetController().SetNumberOfFrames(
                        FFrameNumber(NumFrames));
#else
                    // SequenceLength is deprecated in UE 5.1+ but required for
                    // UE 5.0 compatibility.
                    PRAGMA_DISABLE_DEPRECATION_WARNINGS
                    NewSequence->SequenceLength =
                        static_cast<float>(NumFrames) /
                        static_cast<float>(FrameRate);
                    PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif

                    const int32 AppliedTrackCount =
                        ApplyProceduralBoneTracks(NewSequence, TargetSkeleton,
                                                  *BoneTracksArray, NumFrames);

                    NewSequence->PostEditChange();
                    NewSequence->MarkPackageDirty();
                    if (bShouldSave) {
                      McpSafeOperations::McpSafeAssetSave(NewSequence);
                    }

                    bSuccess = true;
                    Message =
                        FString::Printf(TEXT("Procedural animation '%s' "
                                             "created with %d tracks"),
                                        *Name, AppliedTrackCount);
                    Resp->SetStringField(TEXT("assetPath"),
                                         NewSequence->GetPathName());
                    Resp->SetStringField(TEXT("skeletonPath"),
                                         TargetSkeleton->GetPathName());
                    Resp->SetBoolField(TEXT("existingAsset"), false);
                    Resp->SetNumberField(TEXT("numFrames"), NumFrames);
                    Resp->SetNumberField(TEXT("frameRate"), FrameRate);
                    Resp->SetNumberField(TEXT("appliedTrackCount"),
                                         AppliedTrackCount);
                    McpHandlerUtils::AddVerification(Resp, NewSequence);
                  }
                }
              }
            }
          }
        }
      }
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers
