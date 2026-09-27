#include "Domains/Animation/McpAutomationBridge_AnimationHandlersActionContext.h"
#include "Domains/Animation/Assets/McpAutomationBridge_AnimationHandlersProceduralTracks.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "EditorAssetLibrary.h"
#include "Factories/AnimMontageFactory.h"
#include "Factories/AnimSequenceFactory.h"
#include "Misc/PackageName.h"

namespace McpAnimationHandlers {
bool HandleAnimationCreateAnimationAssetAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  const FString AssetName = GetJsonStringField(Payload, TEXT("name"));
  if (AssetName.IsEmpty()) {
    Context.Fail(TEXT("INVALID_ARGUMENT"), TEXT("name required for create_animation_asset"));
    return false;
  }
  // Published contract names the folder `path`.
  const FString RequestedPath = McpGetFirstStringField(Payload, {TEXT("savePath"), TEXT("path")}).TrimStartAndEnd();
  FString SavePath = RequestedPath.IsEmpty() ? FString(TEXT("/Game/Animations")) : RequestedPath;
  if (!FPackageName::IsValidLongPackageName(SavePath) &&
      !FPackageName::TryConvertFilenameToLongPackageName(RequestedPath, SavePath)) {
    Context.Fail(TEXT("INVALID_ARGUMENT"), TEXT("Invalid savePath for animation asset"));
    return false;
  }
  const FString SkeletonPath = GetJsonStringField(Payload, TEXT("skeletonPath"));
  USkeleton *TargetSkeleton =
      SkeletonPath.IsEmpty() ? nullptr : LoadObject<USkeleton>(nullptr, *SkeletonPath, nullptr, LOAD_NoWarn);
  if (!TargetSkeleton) {
    Context.Fail(SkeletonPath.IsEmpty() ? TEXT("INVALID_ARGUMENT") : TEXT("ASSET_NOT_FOUND"),
                 SkeletonPath.IsEmpty() ? FString(TEXT("skeletonPath is required for create_animation_asset"))
                                        : FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath));
    return false;
  }
  if (!UEditorAssetLibrary::DoesDirectoryExist(SavePath)) {
    UEditorAssetLibrary::MakeDirectory(SavePath);
  }

  const bool bMontage = GetJsonStringField(Payload, TEXT("assetType")).Equals(TEXT("montage"), ESearchCase::IgnoreCase);
  UFactory *Factory = nullptr;
  if (bMontage) {
    UAnimMontageFactory *MontageFactory = NewObject<UAnimMontageFactory>();
    MontageFactory->TargetSkeleton = TargetSkeleton;
    Factory = MontageFactory;
  } else {
    UAnimSequenceFactory *SequenceFactory = NewObject<UAnimSequenceFactory>();
    SequenceFactory->TargetSkeleton = TargetSkeleton;
    Factory = SequenceFactory;
  }
  const FString AssetTypeString = bMontage ? TEXT("Montage") : TEXT("Sequence");
  bool bExisting = false;
  FString Code;
  FString Error;
  UObject *Asset = CreateOrReuseAnimAsset(bMontage ? UAnimMontage::StaticClass() : UAnimSequence::StaticClass(),
                                          Factory, SavePath, AssetName, bExisting, Code, Error);
  if (!Asset) {
    Context.Fail(*Code, Error);
    return false;
  }
  Resp->SetStringField(TEXT("assetPath"), bExisting ? SavePath / AssetName : Asset->GetPathName());
  Resp->SetStringField(TEXT("assetType"), AssetTypeString);
  Resp->SetBoolField(TEXT("existingAsset"), bExisting);
  McpHandlerUtils::AddVerification(Resp, Asset);
  Context.bSuccess = true;
  Context.Message = bExisting ? FString(TEXT("Animation asset already exists - existing asset reused"))
                              : FString::Printf(TEXT("Animation %s created"), *AssetTypeString);
  return false;
}
} // namespace McpAnimationHandlers
