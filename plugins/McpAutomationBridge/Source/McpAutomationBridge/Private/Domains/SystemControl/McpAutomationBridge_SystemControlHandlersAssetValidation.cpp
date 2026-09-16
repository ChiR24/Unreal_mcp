#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersAssetPathCanonical.h"
#include "Domains/SystemControl/McpAutomationBridge_SystemControlHandlersPrivate.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR
#include "EditorAssetLibrary.h"
#endif

namespace McpSystemControlHandlers {

bool HandleValidateAssets(UMcpAutomationBridgeSubsystem* Self,
                          const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload,
                          FSystemControlSocket RequestingSocket) {
#if WITH_EDITOR
  TArray<FString> PathsToValidate;

  const TArray<TSharedPtr<FJsonValue>>* PathsArray = nullptr;
  if (Payload->TryGetArrayField(TEXT("paths"), PathsArray) && PathsArray) {
    for (const TSharedPtr<FJsonValue>& PathValue : *PathsArray) {
      if (PathValue.IsValid() && PathValue->Type == EJson::String) {
        FString Path = PathValue->AsString();
        Path.TrimStartAndEndInline();
        if (!Path.IsEmpty()) {
          PathsToValidate.Add(Path);
        }
      }
    }
  }

  FString SinglePath;
  if (Payload->TryGetStringField(TEXT("assetPath"), SinglePath) ||
      Payload->TryGetStringField(TEXT("path"), SinglePath)) {
    SinglePath.TrimStartAndEndInline();
    if (!SinglePath.IsEmpty()) {
      PathsToValidate.AddUnique(SinglePath);
    }
  }

  if (PathsToValidate.IsEmpty()) {
    Self->SendAutomationError(RequestingSocket, RequestId,
                              TEXT("validate_assets requires paths, assetPath, or path"),
                              TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const bool bRecursive = Payload->HasField(TEXT("recursive"))
      ? GetJsonBoolField(Payload, TEXT("recursive"))
      : true;
  TArray<TSharedPtr<FJsonValue>> Results;
  bool bAllValid = true;

  auto AddValidationResult = [&](const FString& OriginalPath, bool bSuccess,
                                 const FString& Kind, const FString& Message,
                                 int32 AssetCount = INDEX_NONE) {
    TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
    Item->SetStringField(TEXT("path"), OriginalPath);
    Item->SetBoolField(TEXT("success"), bSuccess);
    Item->SetBoolField(TEXT("isValid"), bSuccess);
    Item->SetStringField(TEXT("kind"), Kind);
    Item->SetStringField(TEXT("message"), Message);
    if (AssetCount != INDEX_NONE) {
      Item->SetNumberField(TEXT("assetCount"), AssetCount);
    }
    Results.Add(MakeShared<FJsonValueObject>(Item));
    bAllValid = bAllValid && bSuccess;
  };

  for (const FString& RawPath : PathsToValidate) {
    FString Path = RawPath;
    McpAssetPathCanonical::MapContentRootInline(Path);

    const FString SafePath = SanitizeProjectRelativePath(Path);
    if (SafePath.IsEmpty()) {
      AddValidationResult(RawPath, false, TEXT("invalid"),
                          TEXT("Invalid or unsafe asset path"));
      continue;
    }

    if (UEditorAssetLibrary::DoesAssetExist(SafePath)) {
      UObject* Asset = UEditorAssetLibrary::LoadAsset(SafePath);
      AddValidationResult(
          SafePath, Asset != nullptr, TEXT("asset"),
          Asset ? TEXT("Asset loaded successfully")
                : TEXT("Asset exists but failed to load"));
      continue;
    }

    if (UEditorAssetLibrary::DoesDirectoryExist(SafePath)) {
      TArray<FString> Assets =
          UEditorAssetLibrary::ListAssets(SafePath, bRecursive, false);
      AddValidationResult(SafePath, true, TEXT("directory"),
                          TEXT("Directory exists"), Assets.Num());
      continue;
    }

    AddValidationResult(SafePath, false, TEXT("missing"),
                        TEXT("Asset or directory not found"));
  }

  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), true);
  Result->SetBoolField(TEXT("isValid"), bAllValid);
  Result->SetArrayField(TEXT("results"), Results);
  Result->SetNumberField(TEXT("checkedCount"), Results.Num());

  // Reporting an error threw away the per-asset results[] the handler had
  // already computed -- the caller got "Asset validation failed" and no way to
  // tell which path failed. The validation itself completed; an invalid asset
  // is a finding, not a transport error. Return it as data via isValid.
  int32 InvalidCount = 0;
  for (const TSharedPtr<FJsonValue>& Entry : Results) {
    bool bEntryValid = false;
    if (Entry.IsValid() && Entry->Type == EJson::Object &&
        Entry->AsObject()->TryGetBoolField(TEXT("isValid"), bEntryValid) &&
        !bEntryValid) {
      ++InvalidCount;
    }
  }
  Result->SetNumberField(TEXT("invalidCount"), InvalidCount);
  Self->SendAutomationResponse(
      RequestingSocket, RequestId, true,
      bAllValid ? TEXT("Asset validation completed; all assets valid")
                : FString::Printf(
                      TEXT("Asset validation completed; %d of %d path(s) invalid - see results[]"),
                      InvalidCount, Results.Num()),
      Result, FString());
  return true;
#else
  return false;
#endif
}

}
