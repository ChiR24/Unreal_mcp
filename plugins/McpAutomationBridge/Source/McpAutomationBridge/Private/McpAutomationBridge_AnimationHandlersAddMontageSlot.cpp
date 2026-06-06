#include "McpAutomationBridge_AnimationHandlersActionContext.h"
#include "McpSafeOperations.h"

#include "Animation/AnimMontage.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationAddMontageSlotAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    // Add a slot track to a montage
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    FString SlotName;
    Payload->TryGetStringField(TEXT("slotName"), SlotName);
    if (SlotName.IsEmpty()) {
      SlotName = TEXT("DefaultSlot");
    }

    if (AssetPath.IsEmpty()) {
      Message = TEXT("assetPath required for add_montage_slot");
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

        FSlotAnimationTrack& NewSlot = Montage->AddSlot(FName(*SlotName));
        bSuccess = true;
        Message = FString::Printf(TEXT("Slot '%s' added to montage"), *SlotName);
        Resp->SetStringField(TEXT("assetPath"), AssetPath);
        Resp->SetStringField(TEXT("slotName"), SlotName);

        Montage->MarkPackageDirty();
        McpSafeOperations::McpSafeAssetSave(Montage);
      }
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers
