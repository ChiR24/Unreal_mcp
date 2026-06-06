#include "McpAutomationBridge_AnimationHandlersActionContext.h"
#include "McpSafeOperations.h"

#include "Animation/AnimMontage.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationLinkSectionsAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    // Link montage sections
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    FString FromSection;
    Payload->TryGetStringField(TEXT("fromSection"), FromSection);

    FString ToSection;
    Payload->TryGetStringField(TEXT("toSection"), ToSection);

    if (AssetPath.IsEmpty() || FromSection.IsEmpty() || ToSection.IsEmpty()) {
      Message = TEXT("assetPath, fromSection, and toSection required for link_sections");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      UAnimMontage *Montage = LoadObject<UAnimMontage>(nullptr, *AssetPath);
      if (!Montage) {
        Message = FString::Printf(TEXT("Montage not found: %s"), *AssetPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        int32 FromIndex = Montage->GetSectionIndex(FName(*FromSection));
        int32 ToIndex = Montage->GetSectionIndex(FName(*ToSection));

        if (FromIndex == INDEX_NONE) {
          Message = FString::Printf(TEXT("From section '%s' not found"), *FromSection);
          ErrorCode = TEXT("SECTION_NOT_FOUND");
          Resp->SetStringField(TEXT("error"), Message);
        } else if (ToIndex == INDEX_NONE) {
          Message = FString::Printf(TEXT("To section '%s' not found"), *ToSection);
          ErrorCode = TEXT("SECTION_NOT_FOUND");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          Montage->Modify();

          // Set the NextSectionName in the from section
          FCompositeSection& Section = Montage->GetAnimCompositeSection(FromIndex);
          Section.NextSectionName = FName(*ToSection);

          Montage->MarkPackageDirty();
          McpSafeOperations::McpSafeAssetSave(Montage);

          bSuccess = true;
          Message = FString::Printf(TEXT("Linked '%s' -> '%s'"), *FromSection, *ToSection);
          Resp->SetStringField(TEXT("assetPath"), AssetPath);
          Resp->SetStringField(TEXT("fromSection"), FromSection);
          Resp->SetStringField(TEXT("toSection"), ToSection);
        }
      }
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers
