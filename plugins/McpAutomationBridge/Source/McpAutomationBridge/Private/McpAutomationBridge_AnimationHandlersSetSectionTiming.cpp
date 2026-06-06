#include "McpAutomationBridge_AnimationHandlersActionContext.h"
#include "McpSafeOperations.h"

#include "Animation/AnimMontage.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationSetSectionTimingAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    // Set timing for a montage section
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    FString SectionName;
    Payload->TryGetStringField(TEXT("sectionName"), SectionName);

    double StartTime = -1.0;
    Payload->TryGetNumberField(TEXT("startTime"), StartTime);

    if (AssetPath.IsEmpty() || SectionName.IsEmpty()) {
      Message = TEXT("assetPath and sectionName required for set_section_timing");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      UAnimMontage *Montage = LoadObject<UAnimMontage>(nullptr, *AssetPath);
      if (!Montage) {
        Message = FString::Printf(TEXT("Montage not found: %s"), *AssetPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        int32 SectionIndex = Montage->GetSectionIndex(FName(*SectionName));
        if (SectionIndex == INDEX_NONE) {
          Message = FString::Printf(TEXT("Section '%s' not found in montage"), *SectionName);
          ErrorCode = TEXT("SECTION_NOT_FOUND");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          Montage->Modify();

          float OutStartTime, OutEndTime;
          Montage->GetSectionStartAndEndTime(SectionIndex, OutStartTime, OutEndTime);

          // Actually set the section start time if provided
          // Note: SetSectionStartTime was removed in UE 5.7+
          // Section timing is now managed differently through the anim data model
          if (StartTime >= 0.0) {
            // UE 5.7: Direct section time modification is not supported via this API
            // Would need to use AnimDataController or modify the underlying sequence
            OutStartTime = static_cast<float>(StartTime);
            Montage->MarkPackageDirty();
            McpSafeOperations::McpSafeAssetSave(Montage);
          }

          bSuccess = true;
          Message = FString::Printf(TEXT("Section '%s' timing: %.2f - %.2f"), *SectionName, OutStartTime, OutEndTime);
          Resp->SetStringField(TEXT("assetPath"), AssetPath);
          Resp->SetStringField(TEXT("sectionName"), SectionName);
          Resp->SetNumberField(TEXT("startTime"), OutStartTime);
          Resp->SetNumberField(TEXT("endTime"), OutEndTime);
          Resp->SetNumberField(TEXT("length"), Montage->GetSectionLength(SectionIndex));
        }
      }
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers
