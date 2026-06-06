#include "McpAutomationBridge_AnimationHandlersActionContext.h"
#include "McpSafeOperations.h"
#include "McpVersionCompatibility.h"

#include "Animation/AnimNotifies/AnimNotify_PlaySound.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimSequence.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationAddNotifyAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("animationPath"), AssetPath) ||
        AssetPath.IsEmpty()) {
      Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    }

    FString NotifyName;
    Payload->TryGetStringField(TEXT("notifyName"), NotifyName);

    double Time = 0.0;
    Payload->TryGetNumberField(TEXT("time"), Time);

    if (AssetPath.IsEmpty() || NotifyName.IsEmpty()) {
      Message = TEXT("assetPath and notifyName are required for add_notify");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      UAnimSequenceBase *AnimAsset =
          LoadObject<UAnimSequenceBase>(nullptr, *AssetPath);
      if (!AnimAsset) {
        Message =
            FString::Printf(TEXT("Animation asset not found: %s"), *AssetPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        UAnimSequence *AnimSeq = Cast<UAnimSequence>(AnimAsset);
        if (AnimSeq) {
          // Resolve Notify Class
          UClass *LoadedNotifyClass = nullptr;
          FString SearchName = NotifyName;

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
          // 1. Try exact match (UE 5.1+ API)
          LoadedNotifyClass = UClass::TryFindTypeSlow<UClass>(SearchName);

          // 2. Try with U prefix
          if (!LoadedNotifyClass && !SearchName.StartsWith(TEXT("U"))) {
            LoadedNotifyClass =
                UClass::TryFindTypeSlow<UClass>(TEXT("U") + SearchName);
          }
#else
          LoadedNotifyClass = ResolveClassByName(SearchName);

          // 2. Try with U prefix
          if (!LoadedNotifyClass && !SearchName.StartsWith(TEXT("U"))) {
            LoadedNotifyClass = ResolveClassByName(TEXT("U") + SearchName);
          }
#endif

          // 3. Try standard Engine path variants
          if (!LoadedNotifyClass) {
            // e.g. /Script/Engine.AnimNotify_PlaySound
            LoadedNotifyClass = FindObject<UClass>(
                nullptr,
                *FString::Printf(TEXT("/Script/Engine.%s"), *SearchName));
          }
          if (!LoadedNotifyClass && !SearchName.StartsWith(TEXT("U"))) {
            // e.g. /Script/Engine.UAnimNotify_PlaySound (UE sometimes uses U
            // prefix in code reflection)
            LoadedNotifyClass = FindObject<UClass>(
                nullptr,
                *FString::Printf(TEXT("/Script/Engine.U%s"), *SearchName));
          }

          AnimSeq->Modify();

          FAnimNotifyEvent NewEvent;
          NewEvent.Link(AnimSeq, (float)Time);
          NewEvent.TriggerTimeOffset = GetTriggerTimeOffsetForType(
              EAnimEventTriggerOffsets::OffsetBefore);

          if (LoadedNotifyClass) {
            UAnimNotify *NewNotify =
                NewObject<UAnimNotify>(AnimSeq, LoadedNotifyClass);
            NewEvent.Notify = NewNotify;
            NewEvent.NotifyName = FName(*NotifyName);
          } else {
            // Default simple notify structure
            NewEvent.NotifyName = FName(*NotifyName);
          }

          AnimSeq->Notifies.Add(NewEvent);

          AnimSeq->PostEditChange();
          McpSafeOperations::McpSafeAssetSave(AnimSeq);

          bSuccess = true;
          Message = FString::Printf(TEXT("Added notify '%s' to %s at %.2fs"),
                                    *NotifyName, *AssetPath, Time);
          Resp->SetStringField(TEXT("assetPath"), AssetPath);
          Resp->SetStringField(TEXT("notifyName"), NotifyName);
          Resp->SetStringField(TEXT("notifyClass"),
                               LoadedNotifyClass ? LoadedNotifyClass->GetName()
                                                 : TEXT("None"));
          Resp->SetNumberField(TEXT("time"), Time);
        } else {
          Message = TEXT("Asset is not an AnimSequence (add_notify currently "
                         "supports AnimSequence only)");
          ErrorCode = TEXT("INVALID_TYPE");
          Resp->SetStringField(TEXT("error"), Message);
        }
      }
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers
