#include "McpAutomationBridge_AnimationHandlersActionContext.h"
#include "McpSafeOperations.h"
#include "McpVersionCompatibility.h"

#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimSequence.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationAddNotifyLegacyAction(FActionContext &Context,
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
        UClass *LoadedNotifyClass = nullptr;
        if (!NotifyName.IsEmpty()) {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
          LoadedNotifyClass = UClass::TryFindTypeSlow<UClass>(NotifyName);
#else
          LoadedNotifyClass = ResolveClassByName(NotifyName);
#endif
          if (!LoadedNotifyClass) {
            LoadedNotifyClass = LoadClass<UObject>(nullptr, *NotifyName);
          }
        }

        if (!LoadedNotifyClass) {
          FString ClassName = NotifyName;
          if (!ClassName.StartsWith("U"))
            ClassName = "U" + ClassName;

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
          LoadedNotifyClass = UClass::TryFindTypeSlow<UClass>(ClassName);
#else
          LoadedNotifyClass = ResolveClassByName(ClassName);
#endif

          if (!LoadedNotifyClass) {
            FString EnginePath =
                FString::Printf(TEXT("/Script/Engine.%s"), *NotifyName);
            LoadedNotifyClass = FindObject<UClass>(nullptr, *EnginePath);

            if (!LoadedNotifyClass && !ClassName.Equals(NotifyName)) {
              EnginePath =
                  FString::Printf(TEXT("/Script/Engine.%s"), *ClassName);
              LoadedNotifyClass = FindObject<UClass>(nullptr, *EnginePath);
            }
          }
        }

        if (LoadedNotifyClass) {
          UAnimSequence *AnimSeq = Cast<UAnimSequence>(AnimAsset);
          if (AnimSeq) {
            AnimSeq->Modify();

            FAnimNotifyEvent NewEvent;
            NewEvent.Link(AnimSeq, Time);
            NewEvent.TriggerTimeOffset = GetTriggerTimeOffsetForType(
                EAnimEventTriggerOffsets::OffsetBefore);

            UAnimNotify *NewNotify =
                NewObject<UAnimNotify>(AnimSeq, LoadedNotifyClass);
            NewEvent.Notify = NewNotify;
            NewEvent.NotifyName = FName(*NotifyName);

            AnimSeq->Notifies.Add(NewEvent);
            AnimSeq->PostEditChange();
            McpSafeOperations::McpSafeAssetSave(AnimSeq);

            bSuccess = true;
            Message = FString::Printf(TEXT("Added notify '%s' to %s at %.2fs"),
                                      *NotifyName, *AssetPath, Time);
            Resp->SetStringField(TEXT("assetPath"), AssetPath);
            Resp->SetStringField(TEXT("notifyName"), NotifyName);
            Resp->SetNumberField(TEXT("time"), Time);
          } else {
            Message = TEXT("Asset is not an AnimSequence (Montages not fully "
                           "supported for add_notify yet)");
            ErrorCode = TEXT("INVALID_TYPE");
            Resp->SetStringField(TEXT("error"), Message);
          }
        } else {
          Message =
              FString::Printf(TEXT("Notify class '%s' not found"), *NotifyName);
          ErrorCode = TEXT("CLASS_NOT_FOUND");
          Resp->SetStringField(TEXT("error"), Message);
        }
      }
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers
