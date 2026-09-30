#include "Domains/Animation/McpAutomationBridge_AnimationHandlersActionContext.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintPaths.h"

#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "RenderingThread.h"
#include "Subsystems/AssetEditorSubsystem.h"

namespace McpAnimationHandlers {
bool HandleAnimationCleanupAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    const TArray<TSharedPtr<FJsonValue>> *ArtifactsArray = nullptr;
    if (!Payload->TryGetArrayField(TEXT("artifacts"), ArtifactsArray) ||
        !ArtifactsArray) {
      Message = TEXT("artifacts array required for cleanup");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      TArray<FString> Cleaned;
      TArray<FString> Missing;
      TArray<FString> Failed;

      for (const TSharedPtr<FJsonValue> &Val : *ArtifactsArray) {
        if (!Val.IsValid() || Val->Type != EJson::String) {
          continue;
        }

        const FString ArtifactPath = Val->AsString().TrimStartAndEnd();
        if (ArtifactPath.IsEmpty()) {
          continue;
        }

        if (McpAssetExists(ArtifactPath)) {
// Close editors to ensure asset can be deleted
          if (GEditor) {
            UObject *Asset = LoadObject<UObject>(nullptr, *ArtifactPath);
            if (Asset) {
              if (UAssetEditorSubsystem *AssetEditorSubsystem =
                      GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()) {
                AssetEditorSubsystem->CloseAllEditorsForAsset(Asset);
              }
            }
          }

          // Flush before deleting to release references
          if (GEditor) {
            FlushRenderingCommands();
            GEditor->ForceGarbageCollection(true);
            FlushRenderingCommands();
          }

          if (UEditorAssetLibrary::DeleteAsset(ArtifactPath)) {
            Cleaned.Add(ArtifactPath);
          } else {
            Failed.Add(ArtifactPath);
          }
        } else {
          Missing.Add(ArtifactPath);
        }
      }

      TArray<TSharedPtr<FJsonValue>> CleanedArray;
      for (const FString &Path : Cleaned) {
        CleanedArray.Add(MakeShared<FJsonValueString>(Path));
      }
      if (CleanedArray.Num() > 0) {
        Resp->SetArrayField(TEXT("cleaned"), CleanedArray);
      }
      Resp->SetNumberField(TEXT("cleanedCount"), Cleaned.Num());

      if (Missing.Num() > 0) {
        TArray<TSharedPtr<FJsonValue>> MissingArray;
        for (const FString &Path : Missing) {
          MissingArray.Add(MakeShared<FJsonValueString>(Path));
        }
        Resp->SetArrayField(TEXT("missing"), MissingArray);
      }

      if (Failed.Num() > 0) {
        TArray<TSharedPtr<FJsonValue>> FailedArray;
        for (const FString &Path : Failed) {
          FailedArray.Add(MakeShared<FJsonValueString>(Path));
        }
        Resp->SetArrayField(TEXT("failed"), FailedArray);
      }

      if (Cleaned.Num() > 0 && Failed.Num() == 0) {
        bSuccess = true;
        Message = TEXT("Animation artifacts removed");
      } else if (Failed.Num() > 0) {
        // Actual failure to delete something that exists
        bSuccess = false;
        Context.Fail(TEXT("CLEANUP_PARTIAL"), TEXT("Some animation artifacts could not be removed"));
      } else if (Cleaned.Num() == 0 && Missing.Num() > 0 && Failed.Num() == 0) {
        // All artifacts were missing - not an error, just nothing to do
        // The end state (no artifacts at those paths) is what the user wanted
        bSuccess = true;
        Message = TEXT("No animation artifacts needed removal (all specified paths were missing)");
        Resp->SetBoolField(TEXT("noOp"), true);
      } else {
        bSuccess = false;
        Context.Fail(TEXT("CLEANUP_NO_OP"), TEXT("No animation artifacts were removed"));
      }
    }
    return false;
}
} // namespace McpAnimationHandlers
