#include "McpAutomationBridge_AnimationHandlersActionContext.h"
#include "McpSafeOperations.h"

#include "Animation/AnimMontage.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationAddMontageSectionAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    // Add a section to a montage
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    FString SectionName;
    Payload->TryGetStringField(TEXT("sectionName"), SectionName);

    double StartTime = 0.0;
    Payload->TryGetNumberField(TEXT("startTime"), StartTime);

    if (AssetPath.IsEmpty() || SectionName.IsEmpty()) {
      Message = TEXT("assetPath and sectionName required for add_montage_section");
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

#if WITH_EDITOR
        int32 SectionIndex = Montage->AddAnimCompositeSection(FName(*SectionName), static_cast<float>(StartTime));
        if (SectionIndex != INDEX_NONE) {
          bSuccess = true;
          Message = FString::Printf(TEXT("Section '%s' added at %.2fs"), *SectionName, StartTime);
          Resp->SetStringField(TEXT("assetPath"), AssetPath);
          Resp->SetStringField(TEXT("sectionName"), SectionName);
          Resp->SetNumberField(TEXT("sectionIndex"), SectionIndex);
          Resp->SetNumberField(TEXT("startTime"), StartTime);

          Montage->MarkPackageDirty();
          McpSafeOperations::McpSafeAssetSave(Montage);
        } else {
          Message = FString::Printf(TEXT("Failed to add section '%s' - name may already exist"), *SectionName);
          ErrorCode = TEXT("SECTION_EXISTS");
          Resp->SetStringField(TEXT("error"), Message);
        }
#else
        Message = TEXT("add_montage_section requires editor build");
        ErrorCode = TEXT("NOT_IMPLEMENTED");
        Resp->SetStringField(TEXT("error"), Message);
#endif
      }
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers
