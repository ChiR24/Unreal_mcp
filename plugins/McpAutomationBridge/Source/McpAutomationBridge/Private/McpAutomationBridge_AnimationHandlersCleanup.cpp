#include "McpAutomationBridge_AnimationHandlersActionContext.h"

#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "RenderingThread.h"
#if __has_include("Subsystems/AssetEditorSubsystem.h")
#include "Subsystems/AssetEditorSubsystem.h"
#define MCP_HAS_ASSET_EDITOR_SUBSYSTEM 1
#elif __has_include("AssetEditorSubsystem.h")
#include "AssetEditorSubsystem.h"
#define MCP_HAS_ASSET_EDITOR_SUBSYSTEM 1
#else
#define MCP_HAS_ASSET_EDITOR_SUBSYSTEM 0
#endif

namespace McpAnimationHandlers {
#if WITH_EDITOR
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

        if (UEditorAssetLibrary::DoesAssetExist(ArtifactPath)) {
// Close editors to ensure asset can be deleted
#if MCP_HAS_ASSET_EDITOR_SUBSYSTEM
          if (GEditor) {
            UObject *Asset = LoadObject<UObject>(nullptr, *ArtifactPath);
            if (Asset) {
              if (UAssetEditorSubsystem *AssetEditorSubsystem =
                      GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()) {
                AssetEditorSubsystem->CloseAllEditorsForAsset(Asset);
              }
            }
          }
#endif

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
        Message = TEXT("Some animation artifacts could not be removed");
        ErrorCode = TEXT("CLEANUP_PARTIAL");
        Resp->SetStringField(TEXT("error"), Message);
      } else if (Cleaned.Num() == 0 && Missing.Num() > 0 && Failed.Num() == 0) {
        // All artifacts were missing - not an error, just nothing to do
        // The end state (no artifacts at those paths) is what the user wanted
        bSuccess = true;
        Message = TEXT("No animation artifacts needed removal (all specified paths were missing)");
        Resp->SetBoolField(TEXT("noOp"), true);
      } else {
        bSuccess = false;
        Message = TEXT("No animation artifacts were removed");
        ErrorCode = TEXT("CLEANUP_NO_OP");
        Resp->SetStringField(TEXT("error"), Message);
      }
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers
