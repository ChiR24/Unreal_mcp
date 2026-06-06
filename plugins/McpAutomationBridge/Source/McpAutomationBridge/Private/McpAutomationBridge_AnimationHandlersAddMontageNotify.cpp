#include "McpAutomationBridge_AnimationHandlersActionContext.h"
#include "McpSafeOperations.h"

#include "Animation/AnimMontage.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationAddMontageNotifyAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    // Add a notify to a montage
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    FString NotifyName;
    Payload->TryGetStringField(TEXT("notifyName"), NotifyName);

    double Time = 0.0;
    Payload->TryGetNumberField(TEXT("time"), Time);

    if (AssetPath.IsEmpty() || NotifyName.IsEmpty()) {
      Message = TEXT("assetPath and notifyName required for add_montage_notify");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      UAnimMontage *Montage = LoadObject<UAnimMontage>(nullptr, *AssetPath);
      if (!Montage) {
        Message = FString::Printf(TEXT("Montage not found: %s"), *AssetPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        Montage->Modify();

        FAnimNotifyEvent NewEvent;
        NewEvent.Link(Montage, static_cast<float>(Time));
        NewEvent.TriggerTimeOffset = GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::OffsetBefore);
        NewEvent.NotifyName = FName(*NotifyName);

        Montage->Notifies.Add(NewEvent);
        Montage->MarkPackageDirty();
        McpSafeOperations::McpSafeAssetSave(Montage);

        bSuccess = true;
        Message = FString::Printf(TEXT("Notify '%s' added at %.2fs"), *NotifyName, Time);
        Resp->SetStringField(TEXT("assetPath"), AssetPath);
        Resp->SetStringField(TEXT("notifyName"), NotifyName);
        Resp->SetNumberField(TEXT("time"), Time);
      }
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers
